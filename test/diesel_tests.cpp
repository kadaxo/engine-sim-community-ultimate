#include <gtest/gtest.h>

#include "../include/diesel_injection.h"
#include "../include/gas_system.h"
#include "../include/synthesizer.h"
#include "../include/units.h"

#include <algorithm>
#include <cmath>
#include <vector>

// Diesel injection: the parts that don't need a whole engine. The engine
// itself runs in the game audio tests (DieselEngineTests).

TEST(DieselTests, InjectedFuelBurnsWithTheOxygenItNeeds) {
    GasSystem gas;
    GasSystem::Mix air;
    air.p_fuel = 0;
    air.p_o2 = 0.25;
    air.p_inert = 0.75;
    gas.initialize(units::pressure(40, units::bar), units::volume(50, units::cc), 900.0, air);

    const double o2 = gas.n_o2();
    const double n = gas.n();

    // A little fuel burns completely and uses 17.75 oxygen per fuel
    const double fuel = 0.1 * o2 / 17.75;
    EXPECT_NEAR(gas.burnInjectedFuel(fuel, 17.75), fuel, fuel * 1e-9);
    EXPECT_NEAR(gas.n_o2(), o2 - 17.75 * fuel, o2 * 1e-9);
    EXPECT_GT(gas.n(), n);      // more product molecules than oxygen used
    EXPECT_EQ(gas.n_fuel(), 0.0);
    const GasSystem::Mix mix = gas.mix();
    EXPECT_NEAR(mix.p_fuel + mix.p_o2 + mix.p_inert, 1.0, 1e-12);

    // More fuel than the oxygen left can burn: only what it allows
    const double left = gas.n_o2();
    EXPECT_NEAR(gas.burnInjectedFuel(10 * fuel, 17.75), left / 17.75, left * 1e-9);
    EXPECT_NEAR(gas.n_o2(), 0.0, o2 * 1e-9);
    EXPECT_EQ(gas.burnInjectedFuel(fuel, 17.75), 0.0);
}

TEST(DieselTests, IgnitionDelayIsShorterInHotterDenserAir) {
    DieselInjection injection;
    injection.initialize(DieselInjection::Parameters());

    // Near top dead center of a 16:1 engine: a few crank degrees
    const double typical = injection.ignitionDelayDegrees(units::pressure(50, units::bar), 850.0, 8.0);
    EXPECT_GT(typical, 1.0);
    EXPECT_LT(typical, 15.0);

    EXPECT_LT(injection.ignitionDelayDegrees(units::pressure(70, units::bar), 950.0, 8.0), typical);
    EXPECT_GT(injection.ignitionDelayDegrees(units::pressure(30, units::bar), 750.0, 8.0), typical);

    // Cold, barely compressed air hardly lights at all
    EXPECT_GT(injection.ignitionDelayDegrees(units::pressure(12, units::bar), 500.0, 1.0), 100.0);

    // The script's scale
    DieselInjection::Parameters params;
    params.ignitionDelay = 2.0;
    DieselInjection slower;
    slower.initialize(params);
    EXPECT_NEAR(slower.ignitionDelayDegrees(units::pressure(50, units::bar), 850.0, 8.0), 2 * typical, 1e-9);
}

TEST(DieselTests, GovernorHoldsIdleAndFollowsThePedal) {
    DieselInjection::Parameters params;
    params.idleSpeed = units::rpm(750);
    params.maxSpeed = units::rpm(4500);
    params.startFuel = 0.6;

    DieselInjection injection;
    injection.initialize(params);
    const double dt = 1e-4;
    auto run = [&](double rpm, double seconds) {
        for (int i = 0; i < seconds / dt; ++i) injection.update(dt, units::rpm(rpm));
    };

    // Cranking: the start fuel
    run(200, 0.5);
    EXPECT_DOUBLE_EQ(injection.getFuelDemand(), 0.6);

    // Below the idle speed it adds fuel; above it with the pedal up, none
    run(650, 1.0);
    const double below = injection.getFuelDemand();
    EXPECT_GT(below, 0.1);
    run(1500, 2.0);
    EXPECT_EQ(injection.getFuelDemand(), 0.0);

    // The pedal sets the fuel, and the governor cuts it at the maximum speed
    injection.setPedal(1.0);
    run(3000, 0.5);
    EXPECT_DOUBLE_EQ(injection.getFuelDemand(), 1.0);
    run(4400, 0.5);
    EXPECT_GT(injection.getFuelDemand(), 0.0);
    EXPECT_LT(injection.getFuelDemand(), 1.0);
    run(4600, 0.5);
    EXPECT_EQ(injection.getFuelDemand(), 0.0);
}

namespace {

    // Same input and audio rate, and nothing random or coloured, as in the
    // synthesizer tests; the control channels follow the one exhaust channel
    Synthesizer::Parameters noiseTestParameters(int auxChannels) {
        Synthesizer::Parameters params;
        params.audioBufferSize = 44100;
        params.audioSampleRate = 44100;
        params.inputBufferSize = 44100;
        params.inputChannelCount = 1;
        params.auxChannelCount = auxChannels;
        params.inputSampleRate = 44100;

        Synthesizer::AudioParameters audio;
        audio.airNoise = 0.0f;
        audio.inputSampleNoise = 0.0f;
        audio.dF_F_mix = 0.0f;
        audio.convolution = 0.0f;
        audio.levelerMode = LevelingFilter::Mode::Off;
        params.initialAudioParameters = audio;
        return params;
    }

    // An exhaust tone, with the engine noise channels set by noise(i)
    template <typename NoiseFn>
    std::vector<float> render(int auxChannels, NoiseFn noise) {
        Synthesizer synth;
        synth.initialize(noiseTestParameters(auxChannels));
        std::vector<double> data(1 + auxChannels, 0.0);
        for (int i = 0; i < 8000; ++i) {
            data[0] = 1000.0 * std::sin(2 * 3.14159265358979 * 100.0 * i / 44100.0);
            if (auxChannels >= Synthesizer::AuxChannelCount) {
                noise(i, &data[1 + Synthesizer::ForcedInductionChannelCount]);
            }
            synth.writeInput(data.data());
        }

        std::vector<float> out;
        synth.renderOffline(out);
        synth.destroy();
        return out;
    }

} /* namespace */

TEST(DieselTests, EngineNoiseIsSilentWithoutDieselInjection) {
    // Engines without diesel injection leave the channels at zero: the sound
    // is the same as before the channels existed
    const std::vector<float> before = render(Synthesizer::ForcedInductionChannelCount, [](int, double *) {});
    const std::vector<float> after = render(Synthesizer::AuxChannelCount, [](int, double *n) { n[0] = n[1] = 0; });
    ASSERT_EQ(before.size(), after.size());
    EXPECT_EQ(before, after);
}

TEST(DieselTests, KnocksAndTicksAreHeard) {
    const std::vector<float> quiet = render(Synthesizer::AuxChannelCount, [](int, double *n) { n[0] = n[1] = 0; });

    // A pressure rise every 40 ms (a four-cylinder idling), and a tick
    // just before each
    const std::vector<float> knock = render(Synthesizer::AuxChannelCount, [](int i, double *n) {
        n[0] = (i % 1764 == 100) ? 500.0 : 0.0;
        n[1] = (i % 1764 == 60) ? 50.0 : 0.0;
    });

    ASSERT_EQ(quiet.size(), knock.size());
    double difference = 0;
    for (size_t i = 0; i < quiet.size(); ++i) {
        difference = std::max(difference, static_cast<double>(std::abs(knock[i] - quiet[i])));
    }
    EXPECT_GT(difference, 10.0);
}
