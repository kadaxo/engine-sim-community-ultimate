#include "../include/leveling_filter.h"

#include <algorithm>
#include <cmath>

namespace {
    // Loudness window (LoudnessBlocks blocks of this length); long enough to
    // span a few firing pulses at idle so the gain doesn't pump with them
    constexpr float LoudnessBlockTime = 0.01f;

    // How fast the gain turns down when it gets louder and back up when it
    // gets quieter
    constexpr float AttackTime = 0.01f;
    constexpr float ReleaseTime = 0.15f;

    // How fast the limiter lets go after a peak
    constexpr float LimiterReleaseTime = 0.05f;

    float smoothingCoefficient(float timeConstant, float sampleRate) {
        return 1.0f - std::exp(-1.0f / (timeConstant * sampleRate));
    }
}

LevelingFilter::LevelingFilter() {
    std::fill(m_blockEnergy, m_blockEnergy + LoudnessBlocks, 0.0f);
    m_block = 0;
    m_currentBlockEnergy = 0.0f;
    m_currentBlockSamples = 0;
    m_meanSquare = 0.0f;

    m_gain = 1.0f;
    m_peak = 0.0f;
    m_attenuation = 1.0f;
    p_target = 30000.0f;
    p_loudnessTarget = 10000.0f;
    p_minLevel = 0.0f;
    p_maxLevel = 1.0f;

    setSampleRate(44100.0f);
}

LevelingFilter::~LevelingFilter() {
    /* void */
}

void LevelingFilter::setSampleRate(float sampleRate) {
    m_blockLength = std::max(1, (int)std::lround(LoudnessBlockTime * sampleRate));
    m_attackCoefficient = smoothingCoefficient(AttackTime, sampleRate);
    m_releaseCoefficient = smoothingCoefficient(ReleaseTime, sampleRate);
    m_limiterReleaseCoefficient = 1.0f - smoothingCoefficient(LimiterReleaseTime, sampleRate);
}

float LevelingFilter::f(float sample) {
    m_currentBlockEnergy += sample * sample;
    if (++m_currentBlockSamples >= m_blockLength) {
        m_blockEnergy[m_block] = m_currentBlockEnergy;
        m_block = (m_block + 1) % LoudnessBlocks;
        m_currentBlockEnergy = 0.0f;
        m_currentBlockSamples = 0;

        float energy = 0.0f;
        for (int i = 0; i < LoudnessBlocks; ++i) energy += m_blockEnergy[i];
        m_meanSquare = energy / (LoudnessBlocks * m_blockLength);
    }

    float gain = p_maxLevel;
    if (m_meanSquare > 0) {
        gain = p_loudnessTarget / std::sqrt(m_meanSquare);
    }
    gain = std::clamp(gain, p_minLevel, p_maxLevel);

    const float k = (gain < m_gain) ? m_attackCoefficient : m_releaseCoefficient;
    m_gain += k * (gain - m_gain);

    // Peak limiter
    const float leveled = sample * m_gain;
    m_peak = std::max(std::abs(leveled), m_peak * m_limiterReleaseCoefficient);
    const float limiter = (m_peak > p_target) ? p_target / m_peak : 1.0f;

    m_attenuation = m_gain * limiter;
    return leveled * limiter;
}
