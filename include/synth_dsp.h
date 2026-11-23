#ifndef ATG_ENGINE_SIM_SYNTH_DSP_H
#define ATG_ENGINE_SIM_SYNTH_DSP_H

#include <cstdint>

// Small building blocks shared by the synthesizer's sounds
namespace synth_dsp {

    // RBJ band-pass with unity peak gain
    struct Bandpass {
        float b0 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void set(float f, float q, float fs);
        float process(float x);
    };

    // xorshift32 noise, uniform in [-1, 1]. Each sound has its own, with its
    // own fixed seed: drawing from rand() would shift the random numbers the
    // combustion uses, and so change every engine's sound and power for a
    // given seed.
    struct Noise {
        uint32_t state;

        float next() {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float>(state / 4294967295.0 * 2.0 - 1.0);
        }
    };

} /* namespace synth_dsp */

#endif /* ATG_ENGINE_SIM_SYNTH_DSP_H */
