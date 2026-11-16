#include <gtest/gtest.h>

#include "../include/synthesizer.h"
#include "../include/constants.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

void setupStandardSynthesizer(Synthesizer &synth) {
    Synthesizer::Parameters params;
    params.audioBufferSize = 512 * 16;
    params.audioSampleRate = 16;
    params.inputBufferSize = 256;
    params.inputChannelCount = 8;
    params.inputSampleRate = 32;

    Synthesizer::AudioParameters audioParams;
    audioParams.airNoise = 0.0;
    audioParams.inputSampleNoise = 0.0;
    audioParams.levelerMaxGain = 1.0;
    audioParams.levelerMinGain = 1.0;
    audioParams.dF_F_mix = 0.0;
    params.initialAudioParameters = audioParams;

    synth.initialize(params);
}

TEST(SynthesizerTests, SynthesizerSanityCheck) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);
    synth.destroy();
}

namespace {

    constexpr float PassThroughSampleRate = 44100.0f;
    constexpr int PassThroughAudioBuffer = 44100;
    constexpr int PassThroughChannels = 8;
    constexpr int PassThroughBlock = 256;

    // Same input and audio rate, and every stage that adds randomness or
    // colour (input jitter, air noise, the dF/F mix, convolution and the
    // leveler's gain) turned off, so the output is the summed channels through
    // the fixed filters only.
    void setupPassThroughSynthesizer(Synthesizer &synth) {
        Synthesizer::Parameters params;
        params.audioBufferSize = PassThroughAudioBuffer;
        params.audioSampleRate = PassThroughSampleRate;
        params.inputBufferSize = 44100;
        params.inputChannelCount = PassThroughChannels;
        params.inputSampleRate = PassThroughSampleRate;

        Synthesizer::AudioParameters audioParams;
        audioParams.airNoise = 0.0f;
        audioParams.inputSampleNoise = 0.0f;
        audioParams.dF_F_mix = 0.0f;
        audioParams.convolution = 0.0f;
        audioParams.levelerMode = LevelingFilter::Mode::Off;
        params.initialAudioParameters = audioParams;

        synth.initialize(params);
    }

    void writeTone(Synthesizer &synth, int sample, double amplitude, double frequency) {
        const double v = amplitude * std::sin(
            2 * constants::pi * frequency * sample / PassThroughSampleRate);
        double data[PassThroughChannels];
        for (double &channel : data) channel = v;
        synth.writeInput(data);
    }

} /* namespace */

TEST(SynthesizerTests, OfflineRenderPassesToneAtChannelSum) {
    constexpr int Samples = 22050;
    constexpr double Amplitude = 1000.0;
    constexpr double Frequency = 440.0;

    Synthesizer synth;
    setupPassThroughSynthesizer(synth);
    for (int i = 0; i < Samples; ++i) writeTone(synth, i, Amplitude, Frequency);

    std::vector<float> output;
    const int rendered = synth.renderOffline(output);
    ASSERT_GE(rendered, Samples);
    ASSERT_EQ(rendered, static_cast<int>(output.size()));

    // Skip the first half so the filters have settled
    double sumSquares = 0;
    for (size_t i = output.size() / 2; i < output.size(); ++i) {
        sumSquares += static_cast<double>(output[i]) * output[i];
    }
    const double measured = std::sqrt(2 * sumSquares / (output.size() - output.size() / 2));

    // The eight channels add up. At 440 Hz the input anti-aliasing filter
    // (4th-order Butterworth at 1900 Hz) and the 10 Hz DC filter take almost
    // nothing off.
    const double lowPass = 1 / std::sqrt(1 + std::pow(Frequency / 1900.0, 8));
    const double dcFilter = 1 / std::sqrt(1 + std::pow(10.0 / Frequency, 2));
    const double expected = PassThroughChannels * Amplitude * lowPass * dcFilter;
    EXPECT_NEAR(measured, expected, expected * 0.005);

    synth.destroy();
}

// The real-time path (rendering thread, input and output ring buffers,
// endInputBlock and readAudioOutput) must deliver exactly what an offline
// render of the same input gives, sample for sample and in order.
TEST(SynthesizerTests, RealtimeThreadMatchesOfflineRender) {
    constexpr int Samples = 8192;
    constexpr int Block = PassThroughBlock;

    Synthesizer offline, live;
    setupPassThroughSynthesizer(offline);
    setupPassThroughSynthesizer(live);
    live.startAudioRenderingThread();

    std::vector<int16_t> received;
    const auto drain = [&] {
        int16_t buffer[PassThroughBlock];
        int read;
        do {
            read = live.readAudioOutput(PassThroughBlock, buffer);
            received.insert(received.end(), buffer, buffer + read);
        } while (read == PassThroughBlock);
    };

    for (int i = 0; i < Samples;) {
        for (int j = 0; j < Block; ++j, ++i) {
            writeTone(offline, i, 1000.0, 440.0);
            writeTone(live, i, 1000.0, 440.0);
        }

        // The thread renders only while the output buffer is below its target
        drain();
        live.endInputBlock();
        live.waitProcessed();
    }

    std::vector<float> expected;
    const int rendered = offline.renderOffline(expected);
    const size_t total = static_cast<size_t>(rendered);

    // Rendering finishes on the thread after waitProcessed() returns
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (received.size() < total && std::chrono::steady_clock::now() < deadline) {
        drain();
        std::this_thread::sleep_for(1ms);
    }

    live.endAudioRenderingThread();

    ASSERT_EQ(received.size(), total);
    for (int i = 0; i < rendered; ++i) {
        const long value = std::clamp(std::lround(expected[i]), -32768L, 32767L);
        ASSERT_EQ(received[i], value) << "at sample " << i;
    }

    live.destroy();
    offline.destroy();
}
