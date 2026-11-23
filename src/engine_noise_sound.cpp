#include "../include/engine_noise_sound.h"

#include <algorithm>
#include <cmath>

float EngineNoiseSound::render(float rise, float tick, const Settings &settings) {
    const float x = rise;
    const float t = tick;

    if (x == 0.0f && t == 0.0f && m_tickEnvelope == 0.0f && !m_filtersSet) {
        return 0.0f;
    }

    const float fs = settings.sampleRate;
    if (!m_filtersSet) {
        // The block and head ring around 1.4 and 3 kHz; the injector's
        // needle and solenoid click higher
        m_blockBand1.set(1400.0f, 1.2f, fs);
        m_blockBand2.set(3000.0f, 2.5f, fs);
        m_tickBand.set(6500.0f, 2.0f, fs);
        m_tickRing.set(3600.0f, 14.0f, fs);
        m_filtersSet = true;
    }

    const float convAmount = settings.convolution;
    const float gain = convAmount * settings.exhaustGain + (1 - convAmount);

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
        const float r = m_noise.next();

        ticks = m_tickEnvelope * 1.5f * m_tickBand.process(r);
        m_tickEnvelope *= 1.0f - 1.0f / (0.0008f * fs);
        if (m_tickEnvelope < 1e-6f * std::abs(t) || m_tickEnvelope < 1e-12f) m_tickEnvelope = 0.0f;
    }
    ticks += 4.0f * m_tickRing.process(ring);

    return gain * (block + ticks);
}
