#include "../include/synth_dsp.h"

#include <algorithm>
#include <cmath>

void synth_dsp::Bandpass::set(float f, float q, float fs) {
    const float w0 = 2.0f * 3.14159265f * std::clamp(f, 20.0f, 0.45f * fs) / fs;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * std::cos(w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float synth_dsp::Bandpass::process(float x) {
    const float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    if (std::fpclassify(y1) == FP_SUBNORMAL) y1 = 0;
    return y;
}
