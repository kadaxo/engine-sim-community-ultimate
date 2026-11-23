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
//
// With --crank it checks the starter: the cranking speed it holds with the
// ignition off, then how soon the engine fires and whether it keeps running
// once the starter lets go. --starter-speed <rpm> replaces the script's
// starter speed in any mode.
//
//   boost-probe <main.mr> --crank [--starter-speed <rpm>]
//
// With --combustion it measures the first cylinder's pressure: the peak and
// the fastest rise per crank degree (what makes a diesel knock), averaged
// over the cycles, at idle and on the dyno at <rpm> with the pedal at a
// quarter, half and full.
//
//   boost-probe <main.mr> --combustion <rpm>
//
// --cr prints the cylinders' compression ratio as the simulation has it
// (each one's largest volume over its smallest, over 1 s of the dyno
// turning the crank at 1500 rpm), to
// catch scripts whose head and piston don't add up to the engine they model.
//
//   boost-probe <main.mr> --cr
#include "../include/engine_loader.h"
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
        std::fprintf(stderr, "usage: boost-probe <main.mr> [rpm ...]\n       boost-probe <main.mr> --snap <rpm>\n       boost-probe <main.mr> --idle\n       boost-probe <main.mr> --crank [--starter-speed <rpm>]\n       boost-probe <main.mr> --combustion <rpm>\n       boost-probe <main.mr> --cr\n");
        return 2;
    }

    std::vector<double> rpms;
    double snapRpm = 0;
    bool idleMode = false;
    bool crankMode = false;
    double starterSpeed = 0;
    double combustionRpm = 0;
    bool crMode = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--snap" && i + 1 < argc) snapRpm = std::atof(argv[++i]);
        else if (std::string(argv[i]) == "--idle") idleMode = true;
        else if (std::string(argv[i]) == "--crank") crankMode = true;
        else if (std::string(argv[i]) == "--cr") crMode = true;
        else if (std::string(argv[i]) == "--starter-speed" && i + 1 < argc) starterSpeed = std::atof(argv[++i]);
        else if (std::string(argv[i]) == "--combustion" && i + 1 < argc) combustionRpm = std::atof(argv[++i]);
        else rpms.push_back(std::atof(argv[i]));
    }
    if (rpms.empty()) rpms = { 2000, 3000, 4000, 5000, 6000 };

    // A main script or an engine file from assets/engines, with the app's
    // default car if it sets none
    es_script::Compiler::Output out;
    std::string error;
    if (!engine_loader::loadScript(argv[1], engine_loader::toolOptions(argv[1], argv[0]), &out, std::cerr, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    if (out.engine == nullptr) {
        std::fprintf(stderr, "script produced no engine\n");
        return 1;
    }

    engine_loader::addDefaults(&out);
    Vehicle *vehicle = out.vehicle;
    Transmission *transmission = out.transmission;

    Engine *engine = out.engine;
    Simulator *sim = engine->createSimulator(vehicle, transmission);
    engine->calculateDisplacement();
    sim->setSimulationFrequency(engine->getSimulationFrequency());
    if (starterSpeed > 0) sim->m_starterMotor.m_rotationSpeed = -units::rpm(starterSpeed);

    // One-sample impulse so the synthesizer renders (and drains) without wave files.
    static int16_t impulse[1] = { 32767 };
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        sim->synthesizer().initializeImpulseResponse(impulse, 1, 1.0f, i);
    }

    // The audio is rendered offline after each frame, as engine-sim-export
    // does, so the probe runs as fast as the computer allows instead of in
    // real time; with no latency to regulate, every frame simulates exactly
    // its 1/60 s.
    sim->setTargetSynthesizerLatency(0.0);
    std::vector<float> rendered;
    auto drain = [&]() {
        sim->synthesizer().renderOffline(rendered);
        rendered.clear();
    };

    std::printf("engine \"%s\"  %.2f L\n",
        engine->getName().c_str(),
        engine->getDisplacement() / units::L);

    engine->getIgnitionModule()->m_enabled = true;
    sim->m_starterMotor.m_enabled = true;

    auto frame = [&]() {
        sim->startFrame(1.0 / 60.0);
        while (sim->simulateStep()) { /* run */ }
        sim->endFrame();
        drain();
        if (sim->isUnstable()) {
            std::fprintf(stderr, "the simulation became unstable at %d Hz (try a higher simulation_frequency)\n",
                sim->getSimulationFrequency());
            std::exit(4);
        }
    };

    if (crankMode) {
        // The starter alone: ignition off, throttle closed
        engine->getIgnitionModule()->m_enabled = false;
        double sum = 0, low = 1e9, high = 0, reach = -1;
        int n = 0;
        std::vector<double> trace;
        for (int f = 1; f <= 60 * 4; ++f) {
            engine->setSpeedControl(0.0);
            frame();
            trace.push_back(engine->getRpm());
            if (f > 60 * 2) {
                const double r = engine->getRpm();
                sum += r; ++n;
                low = std::fmin(low, r);
                high = std::fmax(high, r);
            }
        }
        const double cranking = sum / n;
        const bool clockwise = engine->isSpinningCw();
        for (size_t i = 0; i < trace.size(); ++i) {
            if (trace[i] >= 0.9 * cranking) { reach = (i + 1) / 60.0; break; }
        }

        // Ignition on, starter still held until it fires (or 3 s), then let go
        engine->getIgnitionModule()->m_enabled = true;
        const double firedAt = std::fmax(1.3 * cranking, cranking + 200);
        double fired = -1;
        for (int f = 1; f <= 60 * 3; ++f) {
            engine->setSpeedControl(0.0);
            frame();
            if (engine->getRpm() > firedAt) { fired = f / 60.0; break; }
        }
        sim->m_starterMotor.m_enabled = false;
        for (int f = 0; f < 60 * 3; ++f) { engine->setSpeedControl(0.0); frame(); }
        const double after = engine->getRpm();

        std::printf("starter %.0f lb-ft  %.0f rpm\n",
            std::abs(sim->m_starterMotor.m_maxTorque) / units::ft_lb,
            -units::toRpm(sim->m_starterMotor.m_rotationSpeed));
        std::printf("cranking %.0f rpm (%.0f-%.0f), 90%% after %.2f s, %s\n", cranking, low, high, reach,
            clockwise ? "clockwise" : "counter-clockwise");
        std::printf("fired %s%.2f s; 3 s after release %.0f rpm\n",
            fired < 0 ? "never, " : "after ", fired < 0 ? 3.0 : fired, after);

        sim->endAudioRenderingThread();
        sim->destroy();
        return 0;
    }

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

    if (crMode) {
        // The dyno turns the crank at 1500 rpm, so every cylinder goes through
        // whole strokes even if the engine won't idle
        sim->m_dyno.m_enabled = true;
        sim->m_dyno.m_hold = true;
        sim->m_dyno.m_rotationSpeed = units::rpm(1500);
        for (int f = 0; f < 30; ++f) { engine->setSpeedControl(0.0); frame(); }

        const int n = engine->getCylinderCount();
        std::vector<double> vMin(n, 1e9), vMax(n, 0.0);
        for (int f = 0; f < 60; ++f) {
            engine->setSpeedControl(0.0);
            sim->startFrame(1.0 / 60.0);
            while (sim->simulateStep()) {
                for (int i = 0; i < n; ++i) {
                    const double v = engine->getChamber(i)->getVolume();
                    vMin[i] = std::fmin(vMin[i], v);
                    vMax[i] = std::fmax(vMax[i], v);
                }
            }
            sim->endFrame();
            drain();
        }

        double lo = 1e9, hi = 0;
        for (int i = 0; i < n; ++i) {
            lo = std::fmin(lo, vMax[i] / vMin[i]);
            hi = std::fmax(hi, vMax[i] / vMin[i]);
        }
        std::printf("compression ratio %.2f to %.2f, swept %.1f cc per cylinder\n",
            lo, hi, (vMax[0] - vMin[0]) / units::volume(1.0, units::cc));
        sim->destroy();
        return 0;
    }

    if (combustionRpm > 0) {
        // Samples the first cylinder every simulation step for 2 s after
        // 3 s to settle; a cycle is 720 degrees of crank rotation
        CombustionChamber *chamber = engine->getChamber(0);
        auto measure = [&](const char *label, double speedControl) {
            for (int f = 0; f < 60 * 3; ++f) { engine->setSpeedControl(speedControl); frame(); }

            double lastP = chamber->m_system.pressure();
            double angle = 0, cyclePeak = 0, cycleRise = 0;
            double peakSum = 0, riseSum = 0, riseMax = 0;
            int cycles = 0;
            for (int f = 0; f < 60 * 2; ++f) {
                engine->setSpeedControl(speedControl);
                sim->startFrame(1.0 / 60.0);
                while (sim->simulateStep()) {
                    const double p = chamber->m_system.pressure();
                    const double dTheta = std::abs(engine->getOutputCrankshaft()->m_body.v_theta) * sim->getTimestep();
                    const double degrees = dTheta / units::angle(1, units::deg);
                    if (degrees > 0) cycleRise = std::fmax(cycleRise, (p - lastP) / units::bar / degrees);
                    cyclePeak = std::fmax(cyclePeak, p);
                    lastP = p;

                    angle += degrees;
                    if (angle >= 720) {
                        angle -= 720;
                        peakSum += cyclePeak;
                        riseSum += cycleRise;
                        riseMax = std::fmax(riseMax, cycleRise);
                        ++cycles;
                        cyclePeak = cycleRise = 0;
                    }
                }
                sim->endFrame();
                drain();
            }

            std::printf("%-10s %5.0f rpm  peak %6.1f bar  rise %5.2f bar/deg (max %5.2f)  dyno %6.1f Nm  cycles %d\n",
                label, engine->getRpm(),
                cycles > 0 ? peakSum / cycles / units::bar : 0.0,
                cycles > 0 ? riseSum / cycles : 0.0,
                riseMax,
                sim->getFilteredDynoTorque() / units::Nm,
                cycles);
        };

        measure("idle", 0.0);
        sim->m_dyno.m_enabled = true;
        sim->m_dyno.m_hold = true;
        sim->m_dyno.m_rotationSpeed = units::rpm(combustionRpm);
        measure("quarter", 0.25);
        measure("half", 0.5);
        measure("full", 1.0);

        sim->endAudioRenderingThread();
        sim->destroy();
        return 0;
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
                wastegate = fi->hasVariableGeometry() ? fi->getVaneOpening() : fi->getWastegateOpening();
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
                    wastegate = !fi->isTurbocharger() ? fi->getBypassOpening()
                        : fi->hasVariableGeometry() ? fi->getVaneOpening() : fi->getWastegateOpening();
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
