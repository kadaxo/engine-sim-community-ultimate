#ifndef ATG_ENGINE_SIM_ENGINE_NOISE_SOUND_H
#define ATG_ENGINE_SIM_ENGINE_NOISE_SOUND_H

#include "synth_dsp.h"

// A diesel's engine noise, driven by two control channels from the
// simulation, both in the exhaust channels' units (the simulation scales
// them by the same per-cylinder gains): the cylinders' combustion pressure
// rise, which rings the engine block, and the injector ticks' level on the
// steps an injection starts. Both then get the impulse responses' mean gain,
// so the noise sits with the exhaust at any volume setting but doesn't follow
// its loudness: the knock stands out at idle, where the exhaust is quiet, as
// a real diesel's does. Engines without diesel injection leave both at zero,
// which renders exact silence.
class EngineNoiseSound {
    public:
        static constexpr int ChannelCount = 2;

        struct Settings {
            float sampleRate;
            float exhaustGain;      // the impulse responses' mean gain
            float convolution;      // AudioParameters::convolution
        };

        // One output sample from this sample's pressure rise and tick level
        float render(float rise, float tick, const Settings &settings);

    private:
        synth_dsp::Bandpass m_blockBand1, m_blockBand2, m_tickBand, m_tickRing;
        float m_tickEnvelope = 0;
        float m_lastTick = 0;
        synth_dsp::Noise m_noise{ 0x85EBCA6Bu };
        bool m_filtersSet = false;
};

#endif /* ATG_ENGINE_SIM_ENGINE_NOISE_SOUND_H */
