#include <gtest/gtest.h>

#include "../include/spoken_text.h"
#include "../include/units.h"

using namespace spoken_text;

namespace {
    Units imperialUnits() {
        return Units();
    }

    Units metricUnits() {
        Units u;
        u.power = "kW";
        u.torque = "Nm";
        u.speed = "kph";
        u.pressure = "kPa";
        return u;
    }
}

TEST(SpokenTextTests, NumbersDropTrailingZeros) {
    EXPECT_EQ(number(0.42, 2), "0.42");
    EXPECT_EQ(number(0.4261, 2), "0.43");
    EXPECT_EQ(number(3.5, 2), "3.5");
    EXPECT_EQ(number(2.0, 2), "2");
    EXPECT_EQ(number(1249.6), "1250");
    EXPECT_EQ(number(-0.001, 1), "0");
}

TEST(SpokenTextTests, AdjustmentsAreReworded) {
    Message m = fromLogMessage("[Z] - Set volume to 0.420000");
    EXPECT_EQ(m.text, "Volume 0.42");
    EXPECT_EQ(m.category, Category::Adjustment);

    m = fromLogMessage("[N] - Set simulation freq to 12500.000000");
    EXPECT_EQ(m.text, "Simulation frequency 12500 hertz");

    m = fromLogMessage("[G] - Set dyno speed to 3000.000000");
    EXPECT_EQ(m.text, "Dyno speed 3000 rpm");

    m = fromLogMessage("[M] - Set render layer to 2");
    EXPECT_EQ(m.text, "View layer 2");
    EXPECT_EQ(m.category, Category::View);
}

TEST(SpokenTextTests, ThrottleIsInPercent) {
    const Message m = fromLogMessage("Speed control set to 0.100000");
    EXPECT_EQ(m.text, "Throttle 10 percent");
    EXPECT_EQ(m.category, Category::Throttle);
}

TEST(SpokenTextTests, EventsAreReworded) {
    EXPECT_EQ(fromLogMessage("UPSHIFTED TO 3").text, "Gear 3");
    EXPECT_EQ(fromLogMessage("DOWNSHIFTED TO 1").text, "Gear 1");
    EXPECT_EQ(fromLogMessage("SHIFTED TO NEUTRAL").text, "Neutral");
    EXPECT_EQ(fromLogMessage("DYNOMOMETER ENABLED").text, "Dyno on");
    EXPECT_EQ(fromLogMessage("IGNITION DISABLED").text, "Ignition off");
    EXPECT_EQ(fromLogMessage("CLUTCH DEPRESSED").text, "Clutch in");
    EXPECT_EQ(fromLogMessage("SMART SHIFTING ON").text, "Smart shifting on");
    EXPECT_EQ(fromLogMessage("Entered fullscreen mode").text, "Entered fullscreen mode");
    EXPECT_EQ(fromLogMessage("KEY LIST SHOWN").text,
        "Key list shown on screen. The accessibility guide, F4, has every key");
    EXPECT_EQ(fromLogMessage("OPENING THE ENGINE BUILDER IN YOUR WEB BROWSER").text,
        "Opening the engine builder in your web browser");
    EXPECT_EQ(fromLogMessage("Script failed, see error_log.log").category, Category::Silent);
    EXPECT_EQ(fromLogMessage("RPM sync listening on UDP 127.0.0.1:8765").category, Category::Silent);
}

TEST(SpokenTextTests, PeaksAreSpokenInFull) {
    EXPECT_EQ(
        fromLogMessage("180hp @ 6500rpm | 210lb-ft @ 4500rpm").text,
        "Peak power 180 horsepower at 6500 rpm, peak torque 210 pound-feet at 4500 rpm");
    EXPECT_EQ(
        fromLogMessage("101kW @ 6500rpm | 172Nm @ 4000rpm").text,
        "Peak power 101 kilowatts at 6500 rpm, peak torque 172 newton meters at 4000 rpm");
}

TEST(SpokenTextTests, ScriptErrors) {
    EXPECT_EQ(
        scriptError("punto_hgt(12): error V0080: Undefined builtin input\n"),
        "Script error in punto_hgt line 12: Undefined builtin input");

    EXPECT_EQ(
        scriptError(
            "a(3): error P0001: Unexpected token\n"
            "       While instantiating: main(9): engine\n"
            "a(7): error P0002: Something else\n"),
        "Script error in a line 3: Unexpected token. 2 errors in total");

    EXPECT_EQ(scriptError("Can't find file: C:/x/main.mr\n"), "Script file not found: C:/x/main.mr");
    EXPECT_EQ(scriptError(""), "");
}

TEST(SpokenTextTests, RepeatsOfAHeldKeyAreDropped) {
    Announcer a;
    // Held for two seconds: logged every frame, spoken once
    EXPECT_EQ(a.onLogMessage("CLUTCH DEPRESSED", 1.0), "Clutch in");
    for (double t = 1.0 + 1 / 60.0; t < 3.0; t += 1 / 60.0) {
        EXPECT_EQ(a.onLogMessage("CLUTCH DEPRESSED", t), "");
    }

    // Pressed again after a pause
    EXPECT_EQ(a.onLogMessage("CLUTCH DEPRESSED", 4.0), "Clutch in");

    // A different message in between makes the next one new
    EXPECT_EQ(a.onLogMessage("UPSHIFTED TO 2", 4.1), "Gear 2");
    EXPECT_EQ(a.onLogMessage("CLUTCH DEPRESSED", 4.2), "Clutch in");
}

TEST(SpokenTextTests, AdjustmentsWaitUntilTheValueSettles) {
    Announcer a;
    EXPECT_EQ(a.onLogMessage("[Z] - Set volume to 0.500000", 1.0), "");
    EXPECT_EQ(a.onLogMessage("[Z] - Set volume to 0.520000", 1.1), "");
    EXPECT_EQ(a.poll(1.2), "");
    EXPECT_EQ(a.onLogMessage("[Z] - Set volume to 0.540000", 1.3), "");
    EXPECT_EQ(a.poll(1.5), "");
    EXPECT_EQ(a.poll(1.65), "Volume 0.54");
    EXPECT_EQ(a.poll(2.0), "");
}

TEST(SpokenTextTests, VerbosityFiltersMessages) {
    Announcer a;
    EXPECT_EQ(a.onLogMessage("Speed control set to 0.100000", 1.0), "");
    EXPECT_EQ(a.poll(2.0), "");
    EXPECT_EQ(a.onLogMessage("[M] - Set render layer to 1", 2.0), "");

    a.setVerbosity(Verbosity::All);
    EXPECT_EQ(a.onLogMessage("Speed control set to 0.200000", 3.0), "");
    EXPECT_EQ(a.poll(3.5), "Throttle 20 percent");
    EXPECT_EQ(a.onLogMessage("[M] - Set render layer to 2", 4.0), "View layer 2");

    a.setVerbosity(Verbosity::ReadoutsOnly);
    EXPECT_EQ(a.onLogMessage("IGNITION ENABLED", 5.0), "");
    EXPECT_EQ(a.onLogMessage("[Z] - Set volume to 0.300000", 6.0), "");
    EXPECT_EQ(a.poll(7.0), "");
}

TEST(SpokenTextTests, SpeechKeyCyclesToOffAndBackOn) {
    // Silent (no screen reader): J turns speech on at Normal
    SpeechSetting s = speechKey(false, Verbosity::Normal);
    EXPECT_TRUE(s.on);
    EXPECT_EQ(s.verbosity, Verbosity::Normal);
    EXPECT_EQ(s.announcement, "Speech on. Announcements: normal");

    s = speechKey(true, s.verbosity);
    EXPECT_TRUE(s.on);
    EXPECT_EQ(s.verbosity, Verbosity::All);

    s = speechKey(true, s.verbosity);
    EXPECT_TRUE(s.on);
    EXPECT_EQ(s.verbosity, Verbosity::ReadoutsOnly);

    s = speechKey(true, s.verbosity);
    EXPECT_FALSE(s.on);
    EXPECT_EQ(s.announcement, "Speech off. Press J to turn it back on");

    // Off again: the next press turns it back on at Normal
    s = speechKey(false, s.verbosity);
    EXPECT_TRUE(s.on);
    EXPECT_EQ(s.verbosity, Verbosity::Normal);
}

TEST(SpokenTextTests, DynoSummaryFindsBothPeaks) {
    std::vector<DynoPoint> run;
    for (int rpm = 1000; rpm <= 7000; rpm += 50) {
        // Torque peaks at 4000 rpm, power at 6000 rpm
        const double torque = 200.0 - 0.00002 * (rpm - 4000.0) * (rpm - 4000.0);
        const double power = (rpm <= 6000) ? rpm * 10.0 : 60000.0 - (rpm - 6000.0) * 10.0;
        run.push_back({ static_cast<double>(rpm), torque, power });
    }

    const std::string summary = dynoSummary(run, metricUnits());
    EXPECT_EQ(summary,
        "Dyno run finished. Peak power 60 kilowatts at 6000 rpm, "
        "peak torque 200 newton meters at 4000 rpm");

    EXPECT_EQ(dynoSummary({ { 1000, 1, 1 } }, metricUnits()), "");
}

TEST(SpokenTextTests, Readouts) {
    Readings r;
    EXPECT_EQ(engineReadout(r, imperialUnits()), "No engine loaded");

    r.hasEngine = true;
    r.engineName = "Fiat 1.8 16V HGT";
    r.cylinders = 4;
    r.displacement = units::volume(1747, units::cc);
    r.redline = units::rpm(7150);
    r.engineSpeed = units::rpm(2450);
    r.throttle = 0.2;
    r.ignition = true;
    EXPECT_EQ(engineReadout(r, imperialUnits()), "2450 rpm, throttle 20 percent, ignition on");

    EXPECT_EQ(infoReadout(r, metricUnits()), "Fiat 1.8 16V HGT, 4 cylinders, 1.7 liters, redline 7150 rpm");
    EXPECT_EQ(infoReadout(r, imperialUnits()),
        "Fiat 1.8 16V HGT, 4 cylinders, 1.7 liters, 107 cubic inches, redline 7150 rpm");

    r.vehicleSpeed = units::distance(100, units::km) / units::hour;
    r.gear = 2;
    r.clutch = 1.0;
    EXPECT_EQ(vehicleReadout(r, metricUnits()), "100 kilometers per hour, gear 3, clutch engaged");
    r.gear = -1;
    r.clutch = 0.0;
    r.brake = 1.0;
    EXPECT_EQ(vehicleReadout(r, metricUnits()), "100 kilometers per hour, neutral, clutch disengaged, braking");

    r.vehicleSpeed = units::distance(1, units::mile) / units::hour;
    EXPECT_EQ(vehicleReadout(r, imperialUnits()).rfind("1 mile per hour,", 0), 0u);

    EXPECT_EQ(dynoReadout(r, imperialUnits()), "Dyno off");
    r.dynoEnabled = true;
    r.dynoHold = true;
    r.dynoSpeed = units::rpm(3000);
    r.torque = units::torque(150, units::ft_lb);
    r.power = 90 * units::hp;
    EXPECT_EQ(dynoReadout(r, imperialUnits()), "Dyno holding 3000 rpm, 150 pound-feet, 90 horsepower");
}

TEST(SpokenTextTests, SimulationLoadWarnsNearTheLimit) {
    EXPECT_EQ(simulationLoad(0.453), "Simulation load 45 percent");
    EXPECT_EQ(simulationLoad(0.85), "Simulation load 85 percent, close to the limit");
    EXPECT_EQ(simulationLoad(1.004), "Simulation load 100 percent, close to the limit");
    EXPECT_EQ(simulationLoad(1.3), "Simulation load 130 percent, too high for this computer");

    Readings r;
    r.volume = 0.5;
    r.convolution = 1.0;
    r.leveler = "loudness";
    r.simulationFrequency = 10000;
    EXPECT_EQ(audioReadout(r, metricUnits()),
        "Volume 0.5, convolution 1, leveler loudness, simulation frequency 10000 hertz");

    r.frameRate = 59.8;
    r.simulationLoad = 0.45;
    EXPECT_EQ(audioReadout(r, metricUnits()),
        "Volume 0.5, convolution 1, leveler loudness, simulation frequency 10000 hertz, "
        "load 45 percent, 60 frames per second");
}

TEST(SpokenTextTests, ManifoldPressureFollowsTheGaugeUnits) {
    Readings r;
    r.manifoldPressure = units::pressure(1.0, units::atm) - units::pressure(18.0, units::inHg);
    r.airFlow = 0.0;

    EXPECT_EQ(airReadout(r, imperialUnits()).rfind("Manifold vacuum 18 inches of mercury", 0), 0u);
    EXPECT_EQ(airReadout(r, metricUnits()).rfind("Manifold pressure 40 kilopascals", 0), 0u);

    r.manifoldPressure = units::pressure(1.0, units::atm);
    EXPECT_EQ(airReadout(r, imperialUnits()).rfind("Manifold at atmospheric pressure", 0), 0u);
}

TEST(SpokenTextTests, FuelUsesTheSpeedUnitsSystem) {
    Readings r;
    r.fuelConsumed = units::volume(2.0, units::L);
    r.distance = units::distance(25.0, units::km);
    r.fuelRate = units::volume(3.6, units::L) / 3600.0;

    EXPECT_EQ(fuelReadout(r, metricUnits()),
        "Using 3.6 liters per hour, 2 liters used, 25 kilometers driven, average 8 liters per 100 kilometers");
}
