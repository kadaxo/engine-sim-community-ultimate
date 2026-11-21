#ifndef ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H
#define ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H

#include "convolution_filter.h"
#include "leveling_filter.h"
#include "derivative_filter.h"
#include "low_pass_filter.h"
#include "jitter_filter.h"
#include "ring_buffer.h"
#include "butterworth_low_pass_filter.h"

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
        static constexpr int ForcedInductionChannelCount = 10;

        // Control channels after those for a diesel's engine noise, see
        // renderEngineNoise(): the combustion pressure rise and the injector
        // ticks
        static constexpr int EngineNoiseChannelCount = 2;

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

        // Turbocharger and supercharger sounds for one output sample, from
        // the control channels; signal is the engine sound of that sample,
        // whose loudness the sounds follow.
        float renderForcedInduction(int inputSample, float signal);

        // A diesel's engine noise for one output sample: the block ringing
        // from the combustion pressure rise and the injector ticks; signal
        // is the exhaust sound of that sample, whose loudness they follow
        float renderEngineNoise(int inputSample, float signal);

        struct Bandpass {
            float b0 = 0, b2 = 0, a1 = 0, a2 = 0;
            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            void set(float f, float q, float fs);
            float process(float x);
        };

        float fiNoise();

        double m_whinePhase;
        double m_flutterPhase;
        unsigned int m_fiCounter;
        float m_mixEnvelope;
        uint32_t m_fiNoiseState;
        Bandpass m_whistleBand, m_whooshBand, m_flutterBand;
        Bandpass m_blowoffBand1, m_blowoffBand2, m_wastegateBand;

        // Engine noise (diesel injection), with its own noise source
        Bandpass m_blockBand1, m_blockBand2, m_tickBand, m_tickRing;
        float m_tickEnvelope;
        float m_lastTick;
        uint32_t m_engineNoiseState;
        bool m_engineNoiseFiltersSet;

    //protected:
        ButterworthLowPassFilter<float> m_antialiasing;
        LevelingFilter m_levelingFilter;
        InputChannel *m_inputChannels;
        AudioParameters m_audioParameters;
        int m_inputChannelCount;
        int m_auxChannelCount;
        int m_totalChannelCount;
        int m_inputBufferSize;
        int m_inputSamplesRead;
        int m_latency;
        double m_inputWriteOffset;
        double m_lastInputSampleOffset;

        RingBuffer<int16_t> m_audioBuffer;
        int m_audioBufferSize;

        float m_inputSampleRate;
        float m_audioSampleRate;

        std::thread *m_thread;
        std::atomic<bool> m_run;
        bool m_processed;

        std::mutex m_inputLock;
        std::mutex m_lock0;
        std::condition_variable m_cv0;

        ProcessingFilters *m_filters;
};

#endif /* ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H */
