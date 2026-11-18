// Headless probe for forced induction. Compiles an engine script, holds the
// dyno at a series of engine speeds at full throttle and prints charge
// pressure, shaft speed, dyno torque and supercharger drive load.
//
//   boost-probe <main.mr> [rpm ...]
//
// With --snap it checks the response instead: it cruises at <rpm> on the
// dyno, snaps the throttle open, lifts off, then lets the engine idle and
// revs it with no load, printing boost and shaft speed every 0.1 s.
//
//   boost-probe <main.mr> --snap <rpm>
//
// With --idle it starts the engine with no gas, leaves it in neutral with
// the throttle closed for 20 s and fails (exit code 3) if it settles above
// half its redline.
//
//   boost-probe <main.mr> --idle
#include "../scripting/include/compiler.h"
#include "../include/transmission.h"
#include "../include/units.h"
#include "../include/vehicle.h"

#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: boost-probe <main.mr> [rpm ...]\n       boost-probe <main.mr> --snap <rpm>\n       boost-probe <main.mr> --idle\n");
        return 2;
    }

    std::vector<double> rpms;
    double snapRpm = 0;
    bool idleMode = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--snap" && i + 1 < argc) snapRpm = std::atof(argv[++i]);
        else if (std::string(argv[i]) == "--idle") idleMode = true;
        else rpms.push_back(std::atof(argv[i]));
    }
    if (rpms.empty()) rpms = { 2000, 3000, 4000, 5000, 6000 };

    // Resolve imports relative to the script first; the compiler also searches es/
    const piranha::IrPath script(argv[1]);
    piranha::IrPath scriptDir;
    script.getParentPath(&scriptDir);

    es_script::Compiler compiler;
    compiler.initialize({ scriptDir });
    if (!compiler.compile(script, std::cerr)) {
        std::fprintf(stderr, "compile failed\n");
        compiler.destroy();
        return 1;
    }

    const es_script::Compiler::Output out = compiler.execute();
    compiler.destroy();
    if (out.engine == nullptr) {
        std::fprintf(stderr, "script produced no engine\n");
        return 1;
    }

    // Same defaults as EngineSimApplication::loadScript() for scripts that
    // only set an engine
    Vehicle *vehicle = out.vehicle;
    if (vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        vehicle = new Vehicle;
        vehicle->initialize(vehParams);
    }

    Transmission *transmission = out.transmission;
    if (transmission == nullptr) {
        static const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        transmission = new Transmission;
        transmission->initialize(tParams);
    }

    Engine *engine = out.engine;
    Simulator *sim = engine->createSimulator(vehicle, transmission);
    engine->calculateDisplacement();
    sim->setSimulationFrequency(engine->getSimulationFrequency());

    // One-sample impulse so the synthesizer renders (and drains) without wave files.
    static int16_t impulse[1] = { 32767 };
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        sim->synthesizer().initializeImpulseResponse(impulse, 1, 1.0f, i);
    }
    sim->startAudioRenderingThread();

    std::printf("engine \"%s\"  %.2f L\n",
        engine->getName().c_str(),
        engine->getDisplacement() / units::L);

    engine->getIgnitionModule()->m_enabled = true;
    sim->m_starterMotor.m_enabled = true;

    static int16_t audio[4096];
    auto frame = [&]() {
        sim->startFrame(1.0 / 60.0);
        while (sim->simulateStep()) { /* run */ }
        sim->endFrame();
        sim->readAudioOutput(735, audio);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    };

    // Crank until it fires and idles. --idle first starts it the way a player
    // does with no gas: the starter held with the throttle closed until the
    // engine catches (half again its starter speed) or for two seconds.
    if (idleMode) {
        const double caught = 1.5 * units::toRpm(engine->getStarterSpeed());
        for (int i = 0; i < 60 * 2 && engine->getRpm() < caught; ++i) {
            engine->setSpeedControl(0.0);
            frame();
        }
    }
    for (int i = 0; i < 60 * 6 && engine->getRpm() < 600.0; ++i) {
        engine->setSpeedControl(0.2);
        frame();
    }
    sim->m_starterMotor.m_enabled = false;
    std::printf("started: %.0f rpm\n", units::toRpm(engine->getSpeed()));

    ForcedInduction *fi = engine->getForcedInduction();
    if (fi != nullptr) {
        std::printf("%s\n", fi->getDescription().c_str());
    }

    if (idleMode) {
        // Neutral, no throttle: where the engine settles after starting,
        // from the average of the last five seconds
        double settledSum = 0;
        int settledFrames = 0;
        for (int f = 1; f <= 60 * 20; ++f) {
            engine->setSpeedControl(0.0);
            frame();
            if (f > 60 * 15) {
                settledSum += engine->getRpm();
                ++settledFrames;
            }
            if (f % 30 == 0) {
                std::printf("idle %4.1f s  %5.0f rpm  throttle %.3f  map %5.1f kPa\n",
                    f / 60.0, engine->getRpm(), engine->getThrottle(),
                    engine->getManifoldPressure() / units::kPa);
            }
        }

        const double settled = settledSum / settledFrames;
        const double redline = units::toRpm(engine->getRedline());
        sim->endAudioRenderingThread();
        sim->destroy();

        std::printf("settled at %.0f rpm, redline %.0f rpm\n", settled, redline);
        if (settled > 0.5 * redline) {
            std::printf("FAIL: with the throttle closed it runs above half its redline\n");
            return 3;
        }

        return 0;
    }

    if (snapRpm > 0) {
        auto row = [&](const char *phase, double t) {
            double boost = 0, shaft = 0, wastegate = 0, blowoff = 0;
            if (fi != nullptr) {
                boost = fi->getBoost() / units::psi;
                shaft = units::toRpm(fi->getShaftSpeed()) / 1000.0;
                wastegate = fi->getWastegateOpening();
                blowoff = fi->getBlowoffOpening();
            }
            std::printf("%-8s %4.1f s  %5.0f rpm  boost %6.2f psi  shaft %6.1fk rpm  wastegate %4.2f  blow-off %4.2f  map %5.1f kPa  dyno %6.1f lb-ft\n",
                phase, t, units::toRpm(engine->getSpeed()), boost, shaft, wastegate, blowoff,
                engine->getManifoldPressure() / units::kPa,
                sim->getFilteredDynoTorque() / units::ft_lb);
        };

        sim->m_dyno.m_enabled = true;
        sim->m_dyno.m_hold = true;
        sim->m_dyno.m_rotationSpeed = units::rpm(snapRpm);
        for (int f = 0; f < 60 * 4; ++f) { engine->setSpeedControl(0.15); frame(); }
        row("cruise", 0);
        for (int f = 1; f <= 60 * 4; ++f) {
            engine->setSpeedControl(1.0); frame();
            if (f % 6 == 0) row("snap", f / 60.0);
        }
        for (int f = 1; f <= 60 * 3; ++f) {
            engine->setSpeedControl(0.0); frame();
            if (f % 6 == 0) row("lift", f / 60.0);
        }

        // Idle, then rev with no load
        sim->m_dyno.m_enabled = false;
        sim->m_dyno.m_hold = false;
        for (int f = 0; f < 60 * 3; ++f) { engine->setSpeedControl(0.0); frame(); }
        row("idle", 0);
        for (int f = 1; f <= 60 * 1; ++f) {
            engine->setSpeedControl(1.0); frame();
            if (f % 6 == 0) row("free rev", f / 60.0);
            if (engine->getRpm() > 6000) break;
        }

        sim->endAudioRenderingThread();
        sim->destroy();
        return 0;
    }

    sim->m_dyno.m_enabled = true;
    sim->m_dyno.m_hold = true;

    std::printf("%6s %6s %8s %7s %6s %7s %8s %8s %6s\n",
        "rpm", "t(s)", "boost_psi", "map_kPa", "valve", "shaft_k", "chg_C", "dyno_ftlb", "drive");

    for (double target : rpms) {
        sim->m_dyno.m_rotationSpeed = units::rpm(target);
        for (int f = 0; f < 60 * 6; ++f) {
            engine->setSpeedControl(1.0);
            frame();

            if (f % 60 == 59) {
                double boost = 0, wastegate = 0, shaft = 0, charge = 25, drive = 0;
                if (fi != nullptr) {
                    boost = fi->getBoost() / units::psi;
                    wastegate = fi->isTurbocharger() ? fi->getWastegateOpening() : fi->getBypassOpening();
                    shaft = units::toRpm(fi->getShaftSpeed()) / 1000.0;
                    charge = fi->getChargeTemperature() - 273.15;
                    drive = fi->getDriveTorque() / units::ft_lb;
                }
                std::printf("%6.0f %6.1f %8.2f %7.1f %6.2f %7.1f %8.0f %8.1f %6.1f\n",
                    units::toRpm(engine->getSpeed()), (f + 1) / 60.0, boost, engine->getManifoldPressure() / units::kPa, wastegate,
                    shaft, charge, sim->getFilteredDynoTorque() / units::ft_lb, drive);            }
        }
    }

    sim->endAudioRenderingThread();
    sim->destroy();
    return 0;
}
