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
    m_inputChannels = nullptr;
    m_inputChannelCount = 0;
    m_auxChannelCount = 0;
    m_totalChannelCount = 0;
    m_inputBufferSize = 0;
    m_inputWriteOffset = 0.0;
    m_inputSamplesRead = 0;
    m_latency = 0;

    m_audioBufferSize = 0;

    m_inputSampleRate = 0.0;
    m_audioSampleRate = 0.0;

    m_lastInputSampleOffset = 0.0;

    m_run = true;
    m_thread = nullptr;
    m_filters = nullptr;

    m_whinePhase = 0;
    m_flutterPhase = 0;
    m_fiCounter = 0;
    m_mixEnvelope = 0;
    m_fiNoiseState = 0x9E3779B9u;

    m_tickEnvelope = 0;
    m_lastTick = 0;
    m_engineNoiseState = 0x85EBCA6Bu;
    m_engineNoiseFiltersSet = false;
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

    // Forced induction sounds. Their noise has its own fixed seed: drawing
    // it from rand() would shift the random numbers the combustion uses, and
    // so change every engine's sound and power for a given seed.
    m_whinePhase = 0;
    m_flutterPhase = 0;
    m_fiCounter = 0;
    m_mixEnvelope = 0;
    m_fiNoiseState = 0x9E3779B9u;
    m_whistleBand = m_whooshBand = m_flutterBand = Bandpass();
    m_blowoffBand1 = m_blowoffBand2 = m_wastegateBand = Bandpass();

    m_blockBand1 = m_blockBand2 = m_tickBand = m_tickRing = Bandpass();
    m_tickEnvelope = 0;
    m_lastTick = 0;
    m_engineNoiseState = 0x85EBCA6Bu;
    m_engineNoiseFiltersSet = false;

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

    for (int i = 0; i < m_audioBufferSize; ++i) {
        m_audioBuffer.write(0);
    }
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
        const size_t baseIndex = buffer.writeIndex();
        const double distance =
            inputDistance(m_inputWriteOffset, m_lastInputSampleOffset);
        double s =
            inputDistance(baseIndex, m_lastInputSampleOffset);
        for (; s <= distance; s += 1.0) {
            if (s >= m_inputBufferSize) s -= m_inputBufferSize;

            const double f = s / distance;
            const double sample = lastInputSample * (1 - f) + data[i] * f;

            buffer.write(aux
                ? static_cast<float>(sample)
                : m_filters[i].antialiasing.fast_f(static_cast<float>(sample)));
        }

        m_inputChannels[i].lastInputSample = data[i];
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
    signal += renderForcedInduction(inputSample, exhaust);
    signal += renderEngineNoise(inputSample, exhaust);

    return m_antialiasing.fast_f(signal);
}

void Synthesizer::Bandpass::set(float f, float q, float fs) {
    const float w0 = 2.0f * 3.14159265f * std::clamp(f, 20.0f, 0.45f * fs) / fs;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * std::cos(w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float Synthesizer::Bandpass::process(float x) {
    const float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    if (std::fpclassify(y1) == FP_SUBNORMAL) y1 = 0;
    return y;
}

float Synthesizer::fiNoise() {
    // xorshift32, uniform in [-1, 1]
    m_fiNoiseState ^= m_fiNoiseState << 13;
    m_fiNoiseState ^= m_fiNoiseState >> 17;
    m_fiNoiseState ^= m_fiNoiseState << 5;
    return static_cast<float>(m_fiNoiseState / 4294967295.0 * 2.0 - 1.0);
}

// Turbocharger and supercharger sounds (from kyan0s-git/engine-sim), driven
// by control channels from the simulation: [0] shaft or rotor frequency [Hz]
// (times the whistle pitch), [1] whine level (positive displacement) or the
// compressor's once per revolution tone, [2] blow-off flow, [3] compressor
// flow, [4] surge, [5] external wastegate venting, [6] mode (10 * type +
// blow-off type + 100 * full blades + 10000 * splitter blades; types 1
// turbo, 2 Roots, 3 twin-screw, 4 centrifugal; blow-off 0 vented,
// 1 recirculating, 2 none), [7] blade passing tone, [8] splitter passing
// tone and [9] buzz-saw (the compressor's tones as levels relative to
// the reference blade passing tone, see
// ForcedInduction::getCompressorTones()). All follow the loudness of the
// engine: the exhaust channels' units aren't the same loudness from one
// engine script to another, so they can't carry an absolute level.
float Synthesizer::renderForcedInduction(int inputSample, float signal) {
    if (m_auxChannelCount < ForcedInductionChannelCount) return 0.0f;

    const float *aux[ForcedInductionChannelCount];
    for (int k = 0; k < ForcedInductionChannelCount; ++k) {
        aux[k] = m_inputChannels[m_inputChannelCount + k].transferBuffer;
    }

    const int mode = static_cast<int>(std::lround(aux[6][inputSample]));
    const int type = (mode / 10) % 10;
    const int blowoffType = mode % 10;
    const int blades = std::max((mode / 100) % 100, 1);
    const int splitters = mode / 10000;
    if (type == 0) return 0.0f;

    const float volume = m_audioParameters.forcedInductionVolume;
    const float fs = m_audioSampleRate;
    const float envelopeDecay = 1.0f - 1.0f / (0.3f * fs);
    const float floor = 0.08f * m_audioParameters.levelerTarget;
    const double twoPi = 2 * 3.14159265358979323846;

    // The sounds follow the loudness of the engine (with a floor so that a
    // blow-off on a quiet overrun is still heard)
    const float a = std::abs(signal);
    m_mixEnvelope = std::max(a * (1 - envelopeDecay) + m_mixEnvelope * envelopeDecay, 0.0f);
    const float reference = 0.8f * m_mixEnvelope + floor;

    const float shaft = std::max(aux[0][inputSample], 0.0f);
    const float whine = std::max(aux[1][inputSample], 0.0f);
    const float blowoff = std::max(aux[2][inputSample], 0.0f);
    const float flow = std::max(aux[3][inputSample], 0.0f);
    const float surge = std::max(aux[4][inputSample], 0.0f);
    const float wastegate = std::max(aux[5][inputSample], 0.0f);

    // Filter centres follow the shaft speed; updated every 32 samples
    float tonal = 0;
    float compressor = 0;
    if (type == 1 || type == 4) {
        const float bladeFrequency = blades * shaft;
        if ((m_fiCounter++ & 31) == 0) {
            m_whistleBand.set(bladeFrequency, 12.0f, fs);
            m_whooshBand.set(500.0f + 0.35f * std::clamp(400.0f + 2.4f * shaft, 300.0f, 9000.0f), 0.9f, fs);
        }

        // The compressor's tones, all on the shaft's phase: once per
        // revolution (with a little of twice), the blades passing, and the
        // buzz-saw's shaft harmonics below the blade passing frequency, each
        // with its own fixed weight (from the small differences between the
        // blades). Tones near the top of the band fade out.
        m_whinePhase += twoPi * shaft / fs;
        if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
        const double p = m_whinePhase;
        auto fade = [fs](float f) { return std::clamp((0.45f * fs - f) / (0.05f * fs), 0.0f, 1.0f); };

        const float bladePass = std::max(aux[7][inputSample], 0.0f);
        const float splitterPass = std::max(aux[8][inputSample], 0.0f);
        const float buzzSaw = std::max(aux[9][inputSample], 0.0f);
        compressor += whine * fade(shaft) * static_cast<float>(std::sin(p) + 0.3 * std::sin(2 * p));
        compressor += bladePass * fade(bladeFrequency)
            * (static_cast<float>(std::sin(blades * p)) + 2.0f * m_whistleBand.process(fiNoise()));
        if (splitters > 0) {
            const int n = blades + splitters;
            compressor += splitterPass * fade(n * shaft) * static_cast<float>(std::sin(n * p));
        }
        if (buzzSaw > 0.0f) {
            static constexpr float Weights[] = {
                0.9f, 0.5f, 1.0f, 0.4f, 0.7f, 0.8f, 0.3f, 0.6f, 1.0f, 0.5f,
                0.7f, 0.4f, 0.9f, 0.6f, 0.3f, 0.8f, 0.5f, 0.7f, 0.4f, 0.6f
            };
            float buzz = 0, norm = 0;
            for (int k = 2; k < 2 * blades && k - 2 < 20; ++k) {
                if (k == blades) continue;
                const float w = Weights[k - 2] * fade(k * shaft);
                buzz += w * static_cast<float>(std::sin(k * p));
                norm += Weights[k - 2] * Weights[k - 2];
            }
            if (norm > 0) compressor += buzzSaw * buzz / std::sqrt(norm);
        }

        tonal += 0.45f * flow * flow * m_whooshBand.process(fiNoise());
    }
    else {
        // Supercharger rotor whine: lobe passing frequency with harmonics
        const double lobes = (type == 2) ? 6.0 : 5.0;
        const double f = std::min(shaft * lobes, 0.2 * fs);
        m_whinePhase += twoPi * f / fs;
        if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
        const double p = m_whinePhase;
        float tone = 0;
        if (type == 2) {
            tone = static_cast<float>(std::sin(p) + 0.55 * std::sin(2 * p) + 0.35 * std::sin(3 * p) + 0.2 * std::sin(4 * p));
        }
        else {
            tone = static_cast<float>(std::sin(p) + 0.45 * std::sin(3 * p) + 0.25 * std::sin(5 * p));
        }

        if ((m_fiCounter++ & 31) == 0) {
            m_whistleBand.set(static_cast<float>(2.0 * f + 200.0), 6.0f, fs);
        }
        tonal += whine * (0.45f * tone + 1.5f * m_whistleBand.process(fiNoise()));
    }

    // Compressor surge without a blow-off valve: a fluttering chop
    float bursts = 0;
    if (surge > 0.01f) {
        m_flutterPhase += twoPi * (11.0 + 7.0 * surge) / fs;
        if (m_flutterPhase > twoPi) m_flutterPhase -= twoPi;
        const float gate = static_cast<float>(std::pow(std::max(std::sin(m_flutterPhase), 0.0), 3.0));
        if ((m_fiCounter & 31) == 1) m_flutterBand.set(380.0f, 1.6f, fs);
        bursts += 2.0f * surge * gate * m_flutterBand.process(fiNoise());
    }

    // Blow-off: a sharp vented "pssh" or a soft recirculating whoosh
    if (blowoff > 0.001f) {
        if ((m_fiCounter & 31) == 2) {
            m_blowoffBand1.set(blowoffType == 1 ? 1100.0f : 2600.0f, blowoffType == 1 ? 1.2f : 0.7f, fs);
            m_blowoffBand2.set(6500.0f, 1.3f, fs);
        }
        const float r = fiNoise();
        bursts += (blowoffType == 1)
            ? 0.7f * blowoff * m_blowoffBand1.process(r)
            : blowoff * (1.6f * m_blowoffBand1.process(r) + 0.8f * m_blowoffBand2.process(r));
    }

    // External wastegate venting through its own pipe
    if (wastegate > 0.01f) {
        if ((m_fiCounter & 31) == 3) m_wastegateBand.set(1500.0f, 2.5f, fs);
        bursts += 0.6f * wastegate * m_wastegateBand.process(fiNoise());
    }

    return volume * ((tonal + 0.35f * compressor) * (1.1f * m_mixEnvelope + 0.4f * floor)
        + bursts * reference * 1.5f);
}

// A diesel's engine noise, driven by two control channels from the
// simulation, both in the exhaust channels' units (the simulation scales
// them by the same per-cylinder gains): [0] the cylinders' combustion
// pressure rise, which rings the engine block, and [1] the injector ticks'
// level on the steps an injection starts. Both then get the impulse
// responses' mean gain, so the noise sits with the exhaust at any volume
// setting but doesn't follow its loudness: the knock stands out at idle,
// where the exhaust is quiet, as a real diesel's does. Engines without
// diesel injection leave both at zero, which renders exact silence.
float Synthesizer::renderEngineNoise(int inputSample, float signal) {
    (void)signal;
    if (m_auxChannelCount < AuxChannelCount) return 0.0f;

    const float *rise = m_inputChannels[m_inputChannelCount + ForcedInductionChannelCount].transferBuffer;
    const float *tick = m_inputChannels[m_inputChannelCount + ForcedInductionChannelCount + 1].transferBuffer;
    const float x = rise[inputSample];
    const float t = tick[inputSample];

    if (x == 0.0f && t == 0.0f && m_tickEnvelope == 0.0f && !m_engineNoiseFiltersSet) {
        return 0.0f;
    }

    const float fs = m_audioSampleRate;
    if (!m_engineNoiseFiltersSet) {
        // The block and head ring around 1.4 and 3 kHz; the injector's
        // needle and solenoid click higher
        m_blockBand1.set(1400.0f, 1.2f, fs);
        m_blockBand2.set(3000.0f, 2.5f, fs);
        m_tickBand.set(6500.0f, 2.0f, fs);
        m_tickRing.set(3600.0f, 14.0f, fs);
        m_engineNoiseFiltersSet = true;
    }

    float gain = 0;
    for (int i = 0; i < m_inputChannelCount; ++i) gain += m_filters[i].impulseGain;
    gain /= std::max(m_inputChannelCount, 1);
    const float convAmount = m_audioParameters.convolution;
    gain = convAmount * gain + (1 - convAmount);

    const float block = m_blockBand1.process(x) + 0.6f * m_blockBand2.process(x);

    // Ticks: a short burst on each injection start. The control channel
    // ramps up over a few samples; the burst takes its peak.
    float ring = 0.0f;
    if (t > m_lastTick) {
        ring = t - m_lastTick;
        m_tickEnvelope = std::max(m_tickEnvelope, t);
    }
    m_lastTick = t;

    float ticks = 0.0f;
    if (m_tickEnvelope > 0.0f) {
        m_engineNoiseState ^= m_engineNoiseState << 13;
        m_engineNoiseState ^= m_engineNoiseState >> 17;
        m_engineNoiseState ^= m_engineNoiseState << 5;
        const float r = static_cast<float>(m_engineNoiseState / 4294967295.0 * 2.0 - 1.0);

        ticks = m_tickEnvelope * 1.5f * m_tickBand.process(r);
        m_tickEnvelope *= 1.0f - 1.0f / (0.0008f * fs);
        if (m_tickEnvelope < 1e-6f * std::abs(t) || m_tickEnvelope < 1e-12f) m_tickEnvelope = 0.0f;
    }
    ticks += 4.0f * m_tickRing.process(ring);

    return gain * (block + ticks);
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
