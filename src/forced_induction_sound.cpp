#include "../include/forced_induction_sound.h"

#include <algorithm>
#include <cmath>

float ForcedInductionSound::render(const float *channels, float signal, const Settings &settings) {
    const int mode = static_cast<int>(std::lround(channels[6]));
    const int type = (mode / 10) % 10;
    const int blades = std::max((mode / 100) % 100, 1);
    const int splitters = mode / 10000;
    if (type == 0) return 0.0f;

    const float volume = settings.volume;
    const float fs = settings.sampleRate;
    const float envelopeDecay = 1.0f - 1.0f / (0.3f * fs);
    const float floor = 0.08f * settings.levelerTarget;
    const double twoPi = 2 * 3.14159265358979323846;

    // The sounds follow the loudness of the engine (with a floor so that a
    // blow-off on a quiet overrun is still heard)
    const float a = std::abs(signal);
    m_mixEnvelope = std::max(a * (1 - envelopeDecay) + m_mixEnvelope * envelopeDecay, 0.0f);
    const float reference = 0.8f * m_mixEnvelope + floor;

    const float shaft = std::max(channels[0], 0.0f);
    const float whine = std::max(channels[1], 0.0f);
    const float blowoff = std::max(channels[2], 0.0f);
    const float roar = std::max(channels[3], 0.0f);
    const float surge = std::max(channels[4], 0.0f);
    const float wastegate = channels[5];
    const float whoosh = std::max(channels[10], 0.0f);
    const float whooshFrequency = channels[11];
    const float blowoffFrequency = channels[12];
    const float surgeRate = channels[13];

    // Filter centres follow the shaft speed; updated every 32 samples
    float tonal = 0;
    float compressor = 0;
    if (type == 1 || type == 4) {
        const float bladeFrequency = blades * shaft;
        if ((m_counter++ & 31) == 0) {
            m_whistleBand.set(bladeFrequency, 12.0f, fs);
            m_whooshBand.set(std::max(whooshFrequency, 1000.0f), 0.8f, fs);
            m_roarBand.set(1500.0f, 0.6f, fs);
        }

        // The compressor's tones, all on the shaft's phase: once per
        // revolution (with a little of twice), the blades passing, and the
        // buzz-saw's shaft harmonics below the blade passing frequency, each
        // with its own fixed weight (from the small differences between the
        // blades). Tones near the top of the band fade out.
        m_whinePhase += twoPi * shaft / fs;
        if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
        const double p = m_whinePhase;
        auto fade = [fs](float f) { return std::clamp((0.45f * fs - f) / (0.05f * fs), 0.0f, 1.0f); };

        const float bladePass = std::max(channels[7], 0.0f);
        const float splitterPass = std::max(channels[8], 0.0f);
        const float buzzSaw = std::max(channels[9], 0.0f);
        compressor += whine * fade(shaft) * static_cast<float>(std::sin(p) + 0.3 * std::sin(2 * p));
        compressor += bladePass * fade(bladeFrequency)
            * (static_cast<float>(std::sin(blades * p)) + 2.0f * m_whistleBand.process(m_noise.next()));
        if (splitters > 0) {
            const int n = blades + splitters;
            compressor += splitterPass * fade(n * shaft) * static_cast<float>(std::sin(n * p));
        }
        if (buzzSaw > 0.0f) {
            static constexpr float Weights[] = {
                0.9f, 0.5f, 1.0f, 0.4f, 0.7f, 0.8f, 0.3f, 0.6f, 1.0f, 0.5f,
                0.7f, 0.4f, 0.9f, 0.6f, 0.3f, 0.8f, 0.5f, 0.7f, 0.4f, 0.6f
            };
            float buzz = 0, norm = 0;
            for (int k = 2; k < 2 * blades && k - 2 < 20; ++k) {
                if (k == blades) continue;
                const float w = Weights[k - 2] * fade(k * shaft);
                buzz += w * static_cast<float>(std::sin(k * p));
                norm += Weights[k - 2] * Weights[k - 2];
            }
            if (norm > 0) compressor += buzzSaw * buzz / std::sqrt(norm);
        }

        // The air: the compressor's whoosh near surge and the roar of the
        // air rushing in, both broadband
        compressor += 2.0f * whoosh * m_whooshBand.process(m_noise.next());
        compressor += 2.0f * roar * m_roarBand.process(m_noise.next());

        // Worn bearings. Oil whirl isn't locked to the shaft: its pitch
        // wanders by a percent or two. A worn ball bearing's defect tone is
        // rough, and swells once per revolution.
        const float whirl = std::max(channels[14], 0.0f);
        const float whirlRatio = channels[15];
        if (whirl > 0.0f && whirlRatio > 0.0f) {
            m_whirlDrift += (m_faultNoise.next() - m_whirlDrift) * (2.0f * 3.14159265f * 2.0f / fs);
            const float f = whirlRatio * shaft * (1.0f + 2.0f * m_whirlDrift);
            if ((m_counter & 31) == 3) m_whirlBand.set(f, (whirlRatio < 1.0f) ? 30.0f : 8.0f, fs);
            if (whirlRatio < 1.0f) {
                m_whirlPhase += twoPi * f / fs;
                if (m_whirlPhase > twoPi) m_whirlPhase -= twoPi;
                compressor += whirl * fade(f)
                    * (0.8f * static_cast<float>(std::sin(m_whirlPhase)) + 1.5f * m_whirlBand.process(m_faultNoise.next()));
            }
            else {
                compressor += whirl * fade(f) * (1.0f + 0.5f * static_cast<float>(std::sin(p)))
                    * 2.5f * m_whirlBand.process(m_faultNoise.next());
            }
        }
    }
    else {
        // Supercharger rotor whine: lobe passing frequency with harmonics,
        // which fall off faster the more the rotors are twisted
        const float pulses = channels[19];
        const double lobes = (pulses > 0) ? pulses : ((type == 2) ? 6.0 : 5.0);
        const double f = std::min(shaft * lobes, 0.2 * fs);
        m_whinePhase += twoPi * f / fs;
        if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
        const double p = m_whinePhase;
        float tone = 0;
        if (type == 2) {
            const float r = std::clamp(channels[20], 0.0f, 0.95f);
            float w = 1.0f;
            for (int k = 1; k <= 6 && k * f < 0.45 * fs; ++k, w *= r) {
                tone += w * static_cast<float>(std::sin(k * p));
            }
        }
        else {
            tone = static_cast<float>(std::sin(p) + 0.45 * std::sin(3 * p) + 0.25 * std::sin(5 * p));
        }

        if ((m_counter++ & 31) == 0) {
            m_whistleBand.set(static_cast<float>(2.0 * f + 200.0), 6.0f, fs);
        }
        tonal += whine * (0.45f * tone + 1.5f * m_whistleBand.process(m_noise.next()));
    }

    // Supercharger drive gears: a whine at the tooth mesh, with sidebands
    // from the shaft's run-out
    const float gear = std::max(channels[21], 0.0f);
    const float gearFrequency = channels[22];
    if (gear > 0.0f && gearFrequency > 0.0f && gearFrequency < 0.45f * fs) {
        m_gearPhase += twoPi * gearFrequency / fs;
        if (m_gearPhase > twoPi) m_gearPhase -= twoPi;
        m_gearShaftPhase += twoPi * shaft / fs;
        if (m_gearShaftPhase > twoPi) m_gearShaftPhase -= twoPi;
        const float fade = std::clamp((0.45f * fs - gearFrequency) / (0.05f * fs), 0.0f, 1.0f);
        const float mesh = static_cast<float>(std::sin(m_gearPhase)
            + ((2 * gearFrequency < 0.45f * fs) ? 0.3 * std::sin(2 * m_gearPhase) : 0.0));
        tonal += 0.45f * gear * fade * (1.0f + 0.3f * static_cast<float>(std::sin(m_gearShaftPhase))) * mesh;
    }

    // Compressor surge without a blow-off valve: a fluttering chop at the
    // deep surge rate
    float bursts = 0;
    if (surge > 0.01f) {
        m_flutterPhase += twoPi * std::clamp(surgeRate, 4.0f, 40.0f) / fs;
        if (m_flutterPhase > twoPi) m_flutterPhase -= twoPi;
        const float gate = static_cast<float>(std::pow(std::max(std::sin(m_flutterPhase), 0.0), 3.0));
        if ((m_counter & 31) == 1) m_flutterBand.set(380.0f, 1.6f, fs);
        bursts += 2.0f * surge * gate * m_flutterBand.process(m_noise.next());
    }

    // Blow-off: the jet's broadband hiss around its peak, with a wider
    // shoulder above it (a recirculating valve's comes out through the
    // inlet, which the simulation has taken off)
    if (blowoff > 0.001f) {
        if ((m_counter & 31) == 2) {
            m_blowoffBand1.set(std::max(blowoffFrequency, 800.0f), 0.7f, fs);
            m_blowoffBand2.set(std::max(2.5f * blowoffFrequency, 2000.0f), 1.0f, fs);
        }
        const float r = m_noise.next();
        bursts += 0.44f * blowoff * (1.6f * m_blowoffBand1.process(r) + 0.8f * m_blowoffBand2.process(r));
    }

    // A leak in the charge pipe: a jet like the blow-off valve's, steady
    // while the charge is above the outside's pressure
    const float leak = std::max(channels[17], 0.0f);
    if (leak > 0.001f) {
        const float leakFrequency = channels[18];
        if ((m_counter & 31) == 4) {
            m_leakBand1.set(std::max(leakFrequency, 800.0f), 0.7f, fs);
            m_leakBand2.set(std::max(2.5f * leakFrequency, 2000.0f), 1.0f, fs);
        }
        const float r = m_faultNoise.next();
        bursts += 0.44f * leak * (1.6f * m_leakBand1.process(r) + 0.8f * m_leakBand2.process(r));
    }

    // External wastegate: its share of the exhaust pulses, out of a short
    // pipe with no muffler. An open pipe end radiates as a monopole, its
    // pressure following the rate of change of the flow, so the pulses come
    // out as a rasp (rising 6 dB per octave, taken as unity at 500 Hz). The
    // muffled exhaust has the impulse responses' gain; the open pipe takes
    // about 10 dB less off.
    float pipe = 0;
    if (wastegate != 0.0f || m_wastegateLast != 0.0f) {
        const float gain = settings.exhaustGain;
        pipe = 3.0f * gain * (wastegate - m_wastegateLast) * fs / (2.0f * 3.14159265f * 500.0f);
        m_wastegateLast = wastegate;
    }

    // A worn wastegate's flapper: each exhaust pulse throws it across its
    // play. The steady part of the pressure holds it to one side; the
    // pulses riding on it throw it across. It strikes the linkage as a
    // pulse rises through half of the pulses' recent height above the
    // mean, and the seat side, more softly, as it falls back, each strike
    // ringing the flapper and housing's modes (about 2.3, 4.1 and 6.9 kHz).
    // The strike follows the pulse's height, in the exhaust channels' units
    // like the wastegate's pipe above, less where a strong steady flow pins
    // the flapper against the linkage (full load) than where the pulses
    // throw it about freely (idle and cruise).
    const float knock = channels[16];
    float rattle = 0;
    if (knock != 0.0f || m_rattlePeak > 0.0f) {
        if ((m_counter & 31) == 5 || m_rattleBand1.b0 == 0.0f) {
            m_rattleBand1.set(2300.0f, 20.0f, fs);
            m_rattleBand2.set(4100.0f, 25.0f, fs);
            m_rattleBand3.set(6900.0f, 30.0f, fs);
        }
        m_rattleMean += (knock - m_rattleMean) / (0.03f * fs);
        const float pulse = knock - m_rattleMean;
        m_rattlePeak = std::max(std::abs(pulse), m_rattlePeak * (1.0f - 1.0f / (0.05f * fs)));
        if (m_rattlePeak < 1e-9f) m_rattlePeak = 0.0f;
        float strike = 0;
        const bool above = pulse > 0.5f * m_rattlePeak;
        if (above && !m_rattleAbove) strike = m_rattlePeak;
        else if (!above && m_rattleAbove) strike = -0.5f * m_rattlePeak;
        m_rattleAbove = above;

        if (strike != 0.0f) {
            strike *= (1.0f + 0.3f * m_faultNoise.next())
                * m_rattlePeak / (m_rattlePeak + 0.5f * std::abs(m_rattleMean) + 1e-12f);
        }
        const float gain = settings.exhaustGain;
        const float x = 40.0f * gain * strike;
        rattle = m_rattleBand1.process(x) + 0.7f * m_rattleBand2.process(x) + 0.5f * m_rattleBand3.process(x);
    }

    return volume * ((tonal + 0.35f * compressor) * (1.1f * m_mixEnvelope + 0.4f * floor)
        + bursts * reference * 1.5f + pipe + rattle);
}
