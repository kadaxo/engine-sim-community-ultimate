#ifndef ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H
#define ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H

#include "convolution_filter.h"
#include "leveling_filter.h"
#include "derivative_filter.h"
#include "low_pass_filter.h"
#include "jitter_filter.h"
#include "ring_buffer.h"
#include "butterworth_low_pass_filter.h"
#include "forced_induction_sound.h"
#include "engine_noise_sound.h"
#include "listener_model.h"

#include <cstdint>
#include <cinttypes>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>

class Synthesizer {
    public:
        // Control channels a simulation writes after its exhaust channels to
        // drive the turbocharger and supercharger sounds, see
        // renderForcedInduction()
        static constexpr int ForcedInductionChannelCount = ForcedInductionSound::ChannelCount;

        // Control channels after those for a diesel's engine noise, see
        // renderEngineNoise(): the combustion pressure rise and the injector
        // ticks
        static constexpr int EngineNoiseChannelCount = EngineNoiseSound::ChannelCount;

        // Larger inputs are taken as a simulation blowing up (an engine's
        // exhaust channels peak near 1e10; a float overflows at 3e38)
        static constexpr double MaxInputValue = 1e20;

        // All the control channels a simulation writes
        static constexpr int AuxChannelCount = ForcedInductionChannelCount + EngineNoiseChannelCount;

        struct AudioParameters {
            float volume = 1.0f;
            float convolution = 1.0f;
            float dF_F_mix = 0.01f;
            float inputSampleNoise = 0.5f;
            float inputSampleNoiseFrequencyCutoff = 10000.0f;
            float airNoise = 1.0f;
            float airNoiseFrequencyCutoff = 2000.0f;
            float levelerTarget = 30000.0f;             // peak ceiling
            float levelerLoudnessTarget = 10000.0f;     // RMS level the leveler aims for
            float levelerMaxGain = 10.0f;
            float levelerMinGain = 0.00001f;
            LevelingFilter::Mode levelerMode = LevelingFilter::Mode::Loudness;

            // Turbocharger/supercharger whine and blow-off noise
            float forcedInductionVolume = 1.0f;

            // Where the listener stands: 0 beside the car, 1 behind it, 2 in
            // front of it, 3 inside it. The exhaust comes from the back, and
            // the turbo and the engine's own noise from the engine bay (see
            // engineLocation).
            int listenerPosition = 0;

            // Where the engine bay is: 0 at the front, 1 in the middle
            // (behind the seats), 2 at the back with the exhaust
            int engineLocation = 0;
        };

        struct Parameters {
            int inputChannelCount = 1;

            // Control channels written after the exhaust channels: 0,
            // ForcedInductionChannelCount or AuxChannelCount
            int auxChannelCount = 0;
            int inputBufferSize = 1024;
            int audioBufferSize = 44100;
            float inputSampleRate = 10000;
            float audioSampleRate = 44100;
            AudioParameters initialAudioParameters;
        };

        struct InputChannel {
            RingBuffer<float> data;
            float *transferBuffer = nullptr;
            double lastInputSample = 0.0f;
        };

        struct ProcessingFilters {
            ConvolutionFilter convolution;
            DerivativeFilter derivative;
            JitterFilter jitterFilter;
            ButterworthLowPassFilter<float> airNoiseLowPass;
            LowPassFilter inputDcFilter;
            ButterworthLowPassFilter<double> antialiasing;

            // RMS gain of the impulse response (its energy), which the
            // engine noise follows so it sits with the exhaust
            float impulseGain = 1.0f;
        };

    public:
        Synthesizer();
        ~Synthesizer();

        void initialize(const Parameters &p);
        void initializeImpulseResponse(
            const int16_t *impulseResponse,
            unsigned int samples,
            float volume,
            int index);
        void startAudioRenderingThread();
        void endAudioRenderingThread();
        void destroy();

        int readAudioOutput(int samples, int16_t *buffer);

        void writeInput(const double *data);
        void endInputBlock();

        void waitProcessed();

        void audioRenderingThread();
        void renderAudio();

        double getLatency() const;

        int inputDelta(int s1, int s0) const;
        double inputDistance(double s1, double s0) const;

        void setInputSampleRate(double sampleRate);
        double getInputSampleRate() const { return m_inputSampleRate; }

        int16_t renderAudio(int inputOffset);

        // Offline (non-realtime) rendering. Synchronously consumes all pending
        // input and appends the raw synthesized signal to `output`, bypassing
        // the volume control, the automatic leveler and int16 quantization so
        // that the caller can apply a single, consistent gain across many
        // renders. Must not be used while the audio rendering thread runs.
        // Returns the number of samples appended.
        int renderOffline(std::vector<float> &output);

        double getLevelerGain();
        AudioParameters getAudioParameters();
        void setAudioParameters(const AudioParameters &params);

    protected:
        float renderRawSample(int inputSample);

        // The turbocharger and supercharger sounds and a diesel's engine
        // noise for one output sample, from the control channels; signal is
        // the exhaust sound of that sample
        float renderForcedInduction(int inputSample, float signal);
        float renderEngineNoise(int inputSample);

        // The impulse responses' mean gain, which sounds that bypass the
        // exhaust convolution use to sit with it
        float meanImpulseGain() const;

        ForcedInductionSound m_forcedInductionSound;
        EngineNoiseSound m_engineNoiseSound;
        ListenerModel m_listener;

        ButterworthLowPassFilter<float> m_antialiasing;
        LevelingFilter m_levelingFilter;
        InputChannel *m_inputChannels = nullptr;
        AudioParameters m_audioParameters;
        int m_inputChannelCount = 0;
        int m_auxChannelCount = 0;
        int m_totalChannelCount = 0;
        int m_inputBufferSize = 0;
        int m_inputSamplesRead = 0;
        int m_latency = 0;
        double m_inputWriteOffset = 0.0;
        double m_lastInputSampleOffset = 0.0;

        RingBuffer<int16_t> m_audioBuffer;
        int m_audioBufferSize = 0;

        float m_inputSampleRate = 0.0f;
        float m_audioSampleRate = 0.0f;

        std::thread *m_thread = nullptr;
        std::atomic<bool> m_run{ true };
        bool m_processed = true;

        std::mutex m_inputLock;
        std::mutex m_lock0;
        std::condition_variable m_cv0;

        ProcessingFilters *m_filters = nullptr;
};

#endif /* ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H */
