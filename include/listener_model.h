#ifndef ATG_ENGINE_SIM_LISTENER_MODEL_H
#define ATG_ENGINE_SIM_LISTENER_MODEL_H

#include "synth_dsp.h"

// Where the listener stands relative to the car (see
// Synthesizer::AudioParameters::listenerPosition and engineLocation): mixes
// the exhaust, at the back of the car, with the engine bay's sounds (the
// turbo or supercharger and the engine's own noise) by distance, the body's
// shielding and, inside the car, what the body lets through and the cabin's
// modes.
class ListenerModel {
    public:
        // One output sample: 0 beside the car, 1 behind it, 2 in front of it,
        // 3 inside it; the engine bay 0 at the front, 1 in the middle, 2 at
        // the back
        float render(float exhaust, float bay, int listenerPosition, int engineLocation, float sampleRate);

    private:
        float insideTheCar(float exhaust, float bay, int engineLocation, float sampleRate);

        // Behind or in front of the car: the far end's sound, above 1.5 kHz
        float m_farExhaustLow = 0, m_farBayLow = 0;

        // Inside the car: what the body lets through, and the cabin's modes
        float m_insideExhaustLow = 0, m_insideBayLow = 0;
        synth_dsp::Bandpass m_cabinMode1, m_cabinMode2, m_cabinMode3;
        bool m_cabinModesSet = false;
};

#endif /* ATG_ENGINE_SIM_LISTENER_MODEL_H */
