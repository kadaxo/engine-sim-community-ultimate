#include "../include/listener_model.h"

#include <algorithm>
#include <cmath>

float ListenerModel::render(float exhaust, float bay, int listenerPosition, int engineLocation, float sampleRate) {
    float signal;

    // Listener position. The car is 4.5 m long, and behind it or in front of
    // it the listener stands 3 m from its end. The exhaust is at the back;
    // the engine bay at the front (at the end, as the exhaust), in the middle
    // behind the seats (2.75 m from the front) or at the back (4 m from the
    // front, over the rear axle). A source further than the near end's 3 m is
    // down by spreading (3 m over its distance, 8 dB from the far end), and
    // the car's body shields one past the middle of it, taken as another
    // 4 dB above about 1.5 kHz. Beside the car the sources are all about as
    // far away, and the mix is as it is.
    static constexpr float Shielded = 0.63f;
    const float shelf = 1.0f - std::exp(-2.0f * 3.14159265f * 1500.0f / sampleRate);
    auto distant = [shelf](float x, float &low, float gain, bool shielded) {
        if (!shielded) return gain * x;
        low += (x - low) * shelf;
        if (std::fpclassify(low) == FP_SUBNORMAL) low = 0;
        return gain * (low + Shielded * (x - low));
    };

    // The engine bay's distance from behind and in front of the car [m]
    const int location = std::clamp(engineLocation, 0, 2);
    static constexpr float BayFromBehind[] = { 7.5f, 4.75f, 3.5f };
    static constexpr float BayFromFront[] = { 3.0f, 5.75f, 7.0f };
    switch (listenerPosition) {
        case 1: {
            const float d = BayFromBehind[location];
            signal = exhaust + distant(bay, m_farBayLow, 3.0f / d, d > 4.5f);
            break;
        }
        case 2: {
            const float d = BayFromFront[location];
            signal = distant(exhaust, m_farExhaustLow, 0.4f, true)
                + distant(bay, m_farBayLow, 3.0f / d, d > 4.5f);
            break;
        }
        case 3: signal = insideTheCar(exhaust, bay, engineLocation, sampleRate); break;
        default: signal = exhaust + bay; break;
    }

    return signal;
}

// Inside the car. The body's panels and glass let sound through by their
// mass: 15 dB or so is lost at low frequencies, and above a corner the loss
// grows 6 dB an octave (the mass law), about 20 dB at 100 Hz and 40 dB at 1
// to 2 kHz from the exhaust, until the seals' leaks take over and it levels
// off. A front engine bay is behind the firewall, about half as far away as
// from beside the car (6 dB nearer), and the firewall and its pass-throughs
// let more through (a corner of 250 Hz, levelling at 35 dB down) than the
// floor and the boot do for the exhaust at the back (120 Hz, 45 dB down):
// the engine bay's sounds come from in front and the exhaust as a low
// rumble. A mid engine sits just behind the seats, a third as far away as
// from beside (10 dB nearer), and a rear one about as far as a front one;
// both behind a rear bulkhead with no dashboard or heater in front of it,
// which lets more through (400 Hz, 30 dB down). The cabin is a closed box: its first
// modes, along its length (about 2.5 m, 70 Hz), across it (120 Hz) and from
// floor to roof (155 Hz), lift what passes through them by 4 to 8 dB, which
// is the boom when the firing frequency crosses one. All estimates for an
// average car.
float ListenerModel::insideTheCar(float exhaust, float bay, int engineLocation, float sampleRate) {
    const float fs = sampleRate;
    if (!m_cabinModesSet) {
        m_cabinMode1.set(70.0f, 5.0f, fs);
        m_cabinMode2.set(120.0f, 5.0f, fs);
        m_cabinMode3.set(155.0f, 5.0f, fs);
        m_cabinModesSet = true;
    }

    // The bulkhead between the engine bay and the cabin, by where the engine is
    const int location = std::clamp(engineLocation, 0, 2);
    static constexpr float Nearer[] = { 2.0f, 3.0f, 2.0f };
    static constexpr float BulkheadCorner[] = { 250.0f, 400.0f, 400.0f };
    static constexpr float BulkheadLeak[] = { 0.0178f, 0.0316f, 0.0316f };   // -35, -30 dB

    const float twoPi = 2.0f * 3.14159265f;
    const float bayCorner = 1.0f - std::exp(-twoPi * BulkheadCorner[location] / fs);
    const float floorCorner = 1.0f - std::exp(-twoPi * 120.0f / fs);
    m_insideBayLow += (bay - m_insideBayLow) * bayCorner;
    m_insideExhaustLow += (exhaust - m_insideExhaustLow) * floorCorner;
    if (std::fpclassify(m_insideBayLow) == FP_SUBNORMAL) m_insideBayLow = 0;
    if (std::fpclassify(m_insideExhaustLow) == FP_SUBNORMAL) m_insideExhaustLow = 0;

    static constexpr float Through = 0.178f;      // -15 dB
    const float firewall = Nearer[location] * (Through * m_insideBayLow + BulkheadLeak[location] * bay);
    const float floor = Through * m_insideExhaustLow + 0.0056f * exhaust;         // leaks -45 dB
    const float in = firewall + floor;

    // A band-pass of unity peak gain added k times lifts its mode by 1 + k
    return in
        + 1.51f * m_cabinMode1.process(in)      // +8 dB
        + 0.78f * m_cabinMode2.process(in)      // +5 dB
        + 0.58f * m_cabinMode3.process(in);     // +4 dB
}
