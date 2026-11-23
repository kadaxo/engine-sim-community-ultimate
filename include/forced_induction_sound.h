#ifndef ATG_ENGINE_SIM_FORCED_INDUCTION_SOUND_H
#define ATG_ENGINE_SIM_FORCED_INDUCTION_SOUND_H

#include "synth_dsp.h"

#include <cstdint>

// Turbocharger and supercharger sounds (from kyan0s-git/engine-sim), driven
// by control channels from the simulation, one value each per output sample:
// [0] shaft or rotor frequency [Hz] (times the whistle pitch), [1] whine
// level (positive displacement) or the compressor's once per revolution
// tone, [2] blow-off jet, [3] intake roar, [4] surge, [5] the exhaust pulses
// an external wastegate vents (in the exhaust channels' units), [6] mode
// (10 * type + blow-off type + 100 * full blades + 10000 * splitter blades;
// types 1 turbo, 2 Roots, 3 twin-screw, 4 centrifugal; blow-off 0 vented,
// 1 recirculating, 2 none), [7] blade passing tone, [8] splitter passing
// tone, [9] buzz-saw, [10] whoosh, [11] its centre [Hz], [12] the blow-off
// jet's peak [Hz], [13] the deep surge rate [Hz], [14] worn bearings' whirl,
// [15] its frequency over the shaft's, [16] the exhaust pulses that knock a
// worn wastegate's flapper (exhaust channels' units), [17] a boost leak's jet,
// [18] its peak [Hz], [19] rotor pulses per turn, [20] rotor harmonic ratio,
// [21] drive gear whine and [22] its frequency [Hz] (levels relative to the
// reference blade passing tone, see ForcedInduction::getCompressorTones() and
// getAirNoise()). They follow the loudness of the engine: the exhaust
// channels' units aren't the same loudness from one engine script to
// another, so they can't carry an absolute level. The wastegate's pipe is
// the exception: it is the engine's own exhaust pulses, unmuffled.
class ForcedInductionSound {
    public:
        static constexpr int ChannelCount = 23;

        struct Settings {
            float volume;           // AudioParameters::forcedInductionVolume
            float levelerTarget;    // the leveler's peak ceiling, for a floor
            float sampleRate;
            float exhaustGain;      // the impulse responses' mean gain
        };

        // One output sample. `channels` holds this sample's control values;
        // `signal` is the engine sound of the sample, whose loudness the
        // sounds follow.
        float render(const float *channels, float signal, const Settings &settings);

    private:
        double m_whinePhase = 0;
        // Supercharger drive gears: the mesh, and the shaft that modulates it
        double m_gearPhase = 0, m_gearShaftPhase = 0;
        double m_flutterPhase = 0;
        unsigned int m_counter = 0;
        float m_mixEnvelope = 0;
        synth_dsp::Noise m_noise{ 0x9E3779B9u };
        synth_dsp::Bandpass m_whistleBand, m_whooshBand, m_roarBand, m_flutterBand;
        synth_dsp::Bandpass m_blowoffBand1, m_blowoffBand2;
        float m_wastegateLast = 0;

        // Turbo wear and faults, with their own noise source so they leave
        // the other sounds as they were
        synth_dsp::Noise m_faultNoise{ 0x27D4EB2Fu };
        double m_whirlPhase = 0;
        float m_whirlDrift = 0;
        synth_dsp::Bandpass m_whirlBand, m_leakBand1, m_leakBand2;
        synth_dsp::Bandpass m_rattleBand1, m_rattleBand2, m_rattleBand3;
        float m_rattlePeak = 0;
        float m_rattleMean = 0;
        bool m_rattleAbove = false;
};

#endif /* ATG_ENGINE_SIM_FORCED_INDUCTION_SOUND_H */
