#include "../include/synthesizer.h"

#include "../include/utilities.h"
#include "../include/delta.h"

#include <cstdint>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#undef min
#undef max

namespace {
bool AudioDiagEnabled() {
    static const bool enabled = std::getenv("ENGINE_SIM_AUDIO_DIAG") != nullptr;
    return enabled;
}

long long AudioDiagTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
}

Synthesizer::Synthesizer() {
    /* the members have their defaults in the class */
}

Synthesizer::~Synthesizer() {
    assert(m_inputChannels == nullptr);
    assert(m_thread == nullptr);
    assert(m_filters == nullptr);
}

void Synthesizer::initialize(const Parameters &p) {
    m_inputChannelCount = p.inputChannelCount;
    m_auxChannelCount = p.auxChannelCount;
    m_totalChannelCount = p.inputChannelCount + p.auxChannelCount;
    m_inputBufferSize = p.inputBufferSize;
    m_inputWriteOffset = p.inputBufferSize;
    m_audioBufferSize = p.audioBufferSize;
    m_inputSampleRate = p.inputSampleRate;
    m_audioSampleRate = p.audioSampleRate;
    m_audioParameters = p.initialAudioParameters;

    m_inputSamplesRead = 0;
    m_latency = 0;

    m_inputWriteOffset = 0;
    m_processed = true;

    m_audioBuffer.initialize(p.audioBufferSize);
    m_inputChannels = new InputChannel[m_totalChannelCount];
    for (int i = 0; i < m_totalChannelCount; ++i) {
        m_inputChannels[i].transferBuffer = new float[p.inputBufferSize];
        m_inputChannels[i].data.initialize(p.inputBufferSize);
    }

    // The sounds start over (each keeps its state in members with
    // defaults, and its own noise with its own fixed seed)
    m_forcedInductionSound = ForcedInductionSound();
    m_engineNoiseSound = EngineNoiseSound();
    m_listener = ListenerModel();

    m_filters = new ProcessingFilters[p.inputChannelCount];
    for (int i = 0; i < p.inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            m_audioParameters.airNoiseFrequencyCutoff, m_audioSampleRate);

        m_filters[i].derivative.m_dt = 1 / m_audioSampleRate;

        m_filters[i].inputDcFilter.setCutoffFrequency(10.0);
        m_filters[i].inputDcFilter.m_dt = 1 / m_audioSampleRate;

        m_filters[i].jitterFilter.initialize(
            10,
            m_audioParameters.inputSampleNoiseFrequencyCutoff,
            m_audioSampleRate);

        m_filters[i].antialiasing.setCutoffFrequency(1900.0f, m_audioSampleRate);
    }

    m_levelingFilter.setSampleRate(m_audioSampleRate);
    m_levelingFilter.p_target = m_audioParameters.levelerTarget;
    m_levelingFilter.p_loudnessTarget = m_audioParameters.levelerLoudnessTarget;
    m_levelingFilter.p_maxLevel = m_audioParameters.levelerMaxGain;
    m_levelingFilter.p_minLevel = m_audioParameters.levelerMinGain;
    m_antialiasing.setCutoffFrequency(m_audioSampleRate * 0.45f, m_audioSampleRate);

    // No silence is queued up front. The original wrote audioBufferSize
    // zeros, which a RingBuffer of that capacity can't tell from empty, so it
    // did nothing; working, it would have delayed the sound by a full buffer.
}

void Synthesizer::initializeImpulseResponse(
    const int16_t *impulseResponse,
    unsigned int samples,
    float volume,
    int index)
{
    unsigned int clippedLength = 0;
    for (unsigned int i = 0; i < samples; ++i) {
        if (std::abs(impulseResponse[i]) > 100) {
            clippedLength = i + 1;
        }
    }

    const unsigned int sampleCount = std::min(10000U, clippedLength);
    m_filters[index].convolution.initialize(sampleCount);
    double energy = 0;
    for (unsigned int i = 0; i < sampleCount; ++i) {
        const float h = volume * impulseResponse[i] / INT16_MAX;
        m_filters[index].convolution.getImpulseResponse()[i] = h;
        energy += static_cast<double>(h) * h;
    }

    m_filters[index].impulseGain = static_cast<float>(std::sqrt(energy));
}

void Synthesizer::startAudioRenderingThread() {
    m_run = true;
    m_thread = new std::thread(&Synthesizer::audioRenderingThread, this);
}

void Synthesizer::endAudioRenderingThread() {
    if (m_thread != nullptr) {
        m_run = false;
        endInputBlock();

        m_thread->join();
        delete m_thread;

        m_thread = nullptr;
    }
}

void Synthesizer::destroy() {
    m_audioBuffer.destroy();

    for (int i = 0; i < m_totalChannelCount; ++i) {
        delete[] m_inputChannels[i].transferBuffer;
        m_inputChannels[i].data.destroy();
    }

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].convolution.destroy();
    }

    delete[] m_inputChannels;
    delete[] m_filters;

    m_inputChannels = nullptr;
    m_filters = nullptr;

    m_inputChannelCount = 0;
    m_auxChannelCount = 0;
    m_totalChannelCount = 0;
}

int Synthesizer::readAudioOutput(int samples, int16_t *buffer) {
    std::unique_lock<std::mutex> lock(m_lock0);

    const int newDataLength = m_audioBuffer.size();
    if (newDataLength >= samples) {
        m_audioBuffer.readAndRemove(samples, buffer);
    }
    else {
        if (AudioDiagEnabled() && samples > 0) {
            static long long lastReportMs = 0;
            static int eventCount = 0;
            static long long totalMissing = 0;
            static int maxMissing = 0;
            static int minAvailable = 0;

            const long long nowMs = AudioDiagTimeMs();
            const int missing = samples - newDataLength;
            ++eventCount;
            totalMissing += missing;
            maxMissing = std::max(maxMissing, missing);
            minAvailable = (eventCount == 1)
                ? newDataLength
                : std::min(minAvailable, newDataLength);

            if (lastReportMs == 0 || nowMs - lastReportMs >= 1000) {
                std::fprintf(
                    stderr,
                    "ENGINE_SIM_AUDIO_SYNTH_UNDERRUN_SUMMARY t_ms=%lld events=%d total_missing=%lld max_missing=%d min_available=%d last_requested=%d synth_latency_samples=%d audio_buffer_capacity=%d input_rate=%.3f audio_rate=%.3f\n",
                    nowMs,
                    eventCount,
                    totalMissing,
                    maxMissing,
                    minAvailable,
                    samples,
                    m_latency,
                    m_audioBufferSize,
                    static_cast<double>(m_inputSampleRate),
                    static_cast<double>(m_audioSampleRate));

                lastReportMs = nowMs;
                eventCount = 0;
                totalMissing = 0;
                maxMissing = 0;
                minAvailable = 0;
            }
        }

        m_audioBuffer.readAndRemove(newDataLength, buffer);
        memset(
            buffer + newDataLength,
            0,
            sizeof(int16_t) * ((size_t)samples - newDataLength));
    }

    // Wake the rendering thread as the buffer drains, not only at
    // endInputBlock (from danieljsinclair/engine-sim 6bf893e)
    lock.unlock();
    m_cv0.notify_one();
    
    const int samplesConsumed = std::min(samples, newDataLength);

    return samplesConsumed;
}

void Synthesizer::waitProcessed() {
    {
        std::unique_lock<std::mutex> lk(m_lock0);
        m_cv0.wait(lk, [this] { return m_processed; });
    }
}

void Synthesizer::writeInput(const double *data) {
    m_inputWriteOffset += (double)m_audioSampleRate / m_inputSampleRate;
    if (m_inputWriteOffset >= (double)m_inputBufferSize) {
        m_inputWriteOffset -= (double)m_inputBufferSize;
    }

    for (int i = 0; i < m_totalChannelCount; ++i) {
        RingBuffer<float> &buffer = m_inputChannels[i].data;
        const double lastInputSample = m_inputChannels[i].lastInputSample;

        // Control channels are interpolated only; the exhaust channels are
        // also low-pass filtered
        const bool aux = (i >= m_inputChannelCount);
        // A non-finite value (a simulation blowing up) would poison the
        // filters and the convolution for good, and so would one too big
        // for a float (an engine's exhaust channels peak near 1e10)
        const double value =
            (std::isfinite(data[i]) && std::abs(data[i]) < MaxInputValue) ? data[i] : 0.0;
        const size_t baseIndex = buffer.writeIndex();
        const double distance =
            inputDistance(m_inputWriteOffset, m_lastInputSampleOffset);
        double s =
            inputDistance(baseIndex, m_lastInputSampleOffset);
        for (; s <= distance; s += 1.0) {
            if (s >= m_inputBufferSize) s -= m_inputBufferSize;

            const double f = s / distance;
            const double sample = lastInputSample * (1 - f) + value * f;

            buffer.write(aux
                ? static_cast<float>(sample)
                : m_filters[i].antialiasing.fast_f(static_cast<float>(sample)));
        }

        m_inputChannels[i].lastInputSample = value;
    }

    m_lastInputSampleOffset = m_inputWriteOffset;
}

void Synthesizer::endInputBlock() {
    std::unique_lock<std::mutex> lk(m_inputLock);
    // m_processed is the rendering thread's wait condition: changed without
    // m_lock0, a wake-up sent while that thread checks it is lost and both
    // sides wait for each other
    std::unique_lock<std::mutex> lk0(m_lock0);

    for (int i = 0; i < m_totalChannelCount; ++i) {
        m_inputChannels[i].data.removeBeginning(m_inputSamplesRead);
    }

    if (m_inputChannelCount != 0) {
        m_latency = m_inputChannels[0].data.size();
    }
    
    m_inputSamplesRead = 0;
    m_processed = false;

    lk0.unlock();
    lk.unlock();
    m_cv0.notify_one();
}

void Synthesizer::audioRenderingThread() {
    while (m_run) {
        renderAudio();
    }
}

#undef max
void Synthesizer::renderAudio() {
    std::unique_lock<std::mutex> lk0(m_lock0);

    const int targetAudioBufferSamples = std::min(
        m_audioBufferSize,
        std::max(2000, static_cast<int>(std::round(m_audioSampleRate * 0.1f))));

    m_cv0.wait(lk0, [this, targetAudioBufferSamples] {
        const bool inputAvailable =
            m_inputChannels[0].data.size() > 0
            && m_audioBuffer.size() < targetAudioBufferSamples;
        return !m_run || (inputAvailable && !m_processed);
    });

    const int n = std::min(
        std::max(0, targetAudioBufferSamples - (int)m_audioBuffer.size()),
        (int)m_inputChannels[0].data.size());

    for (int i = 0; i < m_totalChannelCount; ++i) {
        m_inputChannels[i].data.read(n, m_inputChannels[i].transferBuffer);
    }
    
    m_inputSamplesRead = n;
    m_processed = true;

    lk0.unlock();

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            static_cast<float>(m_audioParameters.airNoiseFrequencyCutoff), m_audioSampleRate);
        m_filters[i].jitterFilter.setJitterScale(m_audioParameters.inputSampleNoise);
    }

    std::vector<int16_t> renderedAudio(n);
    for (int i = 0; i < n; ++i) {
        renderedAudio[i] = renderAudio(i);
    }

    {
        std::lock_guard<std::mutex> lock(m_lock0);
        for (int i = 0; i < n; ++i) {
            m_audioBuffer.write(renderedAudio[i]);
        }
    }

    m_cv0.notify_one();
}

double Synthesizer::getLatency() const {
    return (double)m_latency / m_audioSampleRate;
}

int Synthesizer::inputDelta(int s1, int s0) const {
    return (s1 < s0)
        ? m_inputBufferSize - s0 + s1
        : s1 - s0;
}

double Synthesizer::inputDistance(double s1, double s0) const {
    return (s1 < s0)
        ? (double)m_inputBufferSize - s0 + s1
        : s1 - s0;
}

void Synthesizer::setInputSampleRate(double sampleRate) {
    if (sampleRate != m_inputSampleRate) {
        std::lock_guard<std::mutex> lock(m_lock0);
        m_inputSampleRate = sampleRate;
    }
}

float Synthesizer::renderRawSample(int inputSample) {
    const float airNoise = m_audioParameters.airNoise;
    const float dF_F_mix = m_audioParameters.dF_F_mix;
    const float convAmount = m_audioParameters.convolution;

    float signal = 0;
    for (int i = 0; i < m_inputChannelCount; ++i) {
        const float jitteredSample =
            m_filters[i].jitterFilter.fast_f(m_inputChannels[i].transferBuffer[inputSample]);

        const float f_in = jitteredSample;
        const float f_dc = m_filters[i].inputDcFilter.fast_f(f_in);
        const float f = f_in - f_dc;
        const float f_p = m_filters[i].derivative.f(f_in);

        const float noise = 2.0 * ((double)rand() / RAND_MAX) - 1.0;
        const float r =
            m_filters->airNoiseLowPass.fast_f(noise);
        const float r_mixed =
            airNoise * r + (1 - airNoise);

        float v_in =
            f_p * dF_F_mix
            + f * r_mixed * (1 - dF_F_mix);
        if (std::fpclassify(v_in) == FP_SUBNORMAL) {
            v_in = 0;
        }

        const float v =
            convAmount * m_filters[i].convolution.f(v_in)
            + (1 - convAmount) * v_in;

        signal += v;
    }

    const float exhaust = signal;
    const float bay = renderForcedInduction(inputSample, exhaust) + renderEngineNoise(inputSample);
    signal = m_listener.render(
        exhaust, bay, m_audioParameters.listenerPosition, m_audioParameters.engineLocation, m_audioSampleRate);

    return m_antialiasing.fast_f(signal);
}

float Synthesizer::meanImpulseGain() const {
    float gain = 0;
    for (int i = 0; i < m_inputChannelCount; ++i) gain += m_filters[i].impulseGain;
    gain /= std::max(m_inputChannelCount, 1);
    return gain;
}

// See ForcedInductionSound for the control channels
float Synthesizer::renderForcedInduction(int inputSample, float signal) {
    if (m_auxChannelCount < ForcedInductionChannelCount) return 0.0f;

    float channels[ForcedInductionChannelCount];
    for (int k = 0; k < ForcedInductionChannelCount; ++k) {
        channels[k] = m_inputChannels[m_inputChannelCount + k].transferBuffer[inputSample];
    }

    ForcedInductionSound::Settings settings;
    settings.volume = m_audioParameters.forcedInductionVolume;
    settings.levelerTarget = m_audioParameters.levelerTarget;
    settings.sampleRate = m_audioSampleRate;
    settings.exhaustGain = meanImpulseGain();
    return m_forcedInductionSound.render(channels, signal, settings);
}

// See EngineNoiseSound: the two channels after the forced induction's
float Synthesizer::renderEngineNoise(int inputSample) {
    if (m_auxChannelCount < AuxChannelCount) return 0.0f;

    const int first = m_inputChannelCount + ForcedInductionChannelCount;
    EngineNoiseSound::Settings settings;
    settings.sampleRate = m_audioSampleRate;
    settings.exhaustGain = meanImpulseGain();
    settings.convolution = m_audioParameters.convolution;
    return m_engineNoiseSound.render(
        m_inputChannels[first].transferBuffer[inputSample],
        m_inputChannels[first + 1].transferBuffer[inputSample],
        settings);
}

int16_t Synthesizer::renderAudio(int inputSample) {
    const float signal = renderRawSample(inputSample);

    m_levelingFilter.p_target = m_audioParameters.levelerTarget;
    m_levelingFilter.p_loudnessTarget = m_audioParameters.levelerLoudnessTarget;
    m_levelingFilter.p_mode = m_audioParameters.levelerMode;
    const float v_leveled = m_levelingFilter.f(signal) * m_audioParameters.volume;
    int r_int = std::lround(v_leveled);
    if (r_int > INT16_MAX) {
        r_int = INT16_MAX;
    }
    else if (r_int < INT16_MIN) {
        r_int = INT16_MIN;
    }

    return static_cast<int16_t>(r_int);
}

int Synthesizer::renderOffline(std::vector<float> &output) {
    assert(m_thread == nullptr);
    if (m_inputChannelCount == 0) return 0;

    std::lock_guard<std::mutex> lock(m_inputLock);

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            static_cast<float>(m_audioParameters.airNoiseFrequencyCutoff), m_audioSampleRate);
        m_filters[i].jitterFilter.setJitterScale(m_audioParameters.inputSampleNoise);
    }

    int total = 0;
    while (m_inputChannels[0].data.size() > 0) {
        const int n = std::min(
            m_inputBufferSize,
            static_cast<int>(m_inputChannels[0].data.size()));

        for (int i = 0; i < m_totalChannelCount; ++i) {
            m_inputChannels[i].data.read(n, m_inputChannels[i].transferBuffer);
            m_inputChannels[i].data.removeBeginning(n);
        }

        output.reserve(output.size() + n);
        for (int i = 0; i < n; ++i) {
            output.push_back(renderRawSample(i));
        }

        total += n;
    }

    m_inputSamplesRead = 0;
    m_latency = 0;

    return total;
}

double Synthesizer::getLevelerGain() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_levelingFilter.getAttenuation();
}

Synthesizer::AudioParameters Synthesizer::getAudioParameters() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_audioParameters;
}

void Synthesizer::setAudioParameters(const AudioParameters &params) {
    std::lock_guard<std::mutex> lock(m_lock0);
    m_audioParameters = params;
}
