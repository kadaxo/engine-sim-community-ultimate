#ifndef ATG_ENGINE_SIM_LEVELING_FILTER_H
#define ATG_ENGINE_SIM_LEVELING_FILTER_H

#include "filter.h"

#include "function.h"

class LevelingFilter : public Filter {
    public:
        enum class Mode {
            // Steady loudness: a slow gain follows the signal's RMS level
            // towards p_loudnessTarget (within p_minLevel..p_maxLevel), and a
            // peak limiter keeps every sample within p_target
            Loudness,

            // The original leveler: scales the recent peak to p_target
            Peak,

            // Fixed unity gain, peak limiter only
            Off
        };

    public:
        LevelingFilter();
        virtual ~LevelingFilter();

        void setSampleRate(float sampleRate);

        virtual float f(float sample);
        float getAttenuation() const { return m_attenuation; }

    protected:
        float peakLevel(float sample);
        float limit(float sample);

    protected:
        // Loudness is the mean square over a sliding window of short blocks,
        // so a loud stretch stops counting once it leaves the window
        static constexpr int LoudnessBlocks = 30;
        float m_blockEnergy[LoudnessBlocks];
        int m_block;
        float m_currentBlockEnergy;
        int m_currentBlockSamples;
        int m_blockLength;
        float m_meanSquare;

        float m_gain;
        float m_limiterPeak;
        float m_attenuation;

        float m_attackCoefficient;
        float m_releaseCoefficient;
        float m_limiterReleaseCoefficient;

        // Mode::Peak
        float m_peak;
        float m_peakAttenuation;

    public:
        Mode p_mode;
        float p_maxLevel;
        float p_minLevel;
        float p_target;
        float p_loudnessTarget;
};

#endif /* ATG_ENGINE_SIM_LEVELING_FILTER_H */
