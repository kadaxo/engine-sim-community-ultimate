// Headless probe for gear shifting. Compiles an engine script, drives the
// throttle and clutch the way EngineSimApplication does for the keyboard and
// runs a few shift scenarios, printing whether each shift finishes, then
// brakes to a stop and prints the deceleration.
//
//   shift-probe <main.mr> [off]   (off: smart shifting disabled)
#include "../include/engine_loader.h"
#include "../include/transmission.h"
#include "../include/units.h"
#include "../include/vehicle.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
    struct Keys {
        double throttle = -1.0; // Q/W/E/R value held, or -1 for none
        bool up = false;
        bool down = false;
        bool clutch = false; // Shift held
        bool brake = false; // . held
    };

    class Driver {
    public:
        Driver(Simulator *sim, Engine *engine, Transmission *transmission)
            : m_sim(sim), m_engine(engine), m_transmission(transmission) {}

        // One 60 Hz frame of EngineSimApplication::processEngineInput()
        void frame(const Keys &keys) {
            const double dt = 1.0 / 60.0;

            m_engine->setTargetSpeedControl(keys.throttle >= 0 ? keys.throttle : 0.0);

            double shiftThrottle;
            if (m_transmission->getShiftThrottle(&shiftThrottle)) {
                m_engine->setSpeedControl(shiftThrottle * 0.5 + 0.5 * m_engine->getSpeedControl());
            }
            else {
                m_engine->updateSpeedControl();
            }

            if (keys.up) m_transmission->upshift();
            else if (keys.down) m_transmission->downshift();

            if (keys.clutch) {
                m_transmission->setTargetClutchPressure(0.0);
            }
            else if (!m_transmission->isShifting()) {
                m_transmission->setTargetClutchPressure(1.0);
            }

            const double clutchRC = 0.001;
            m_transmission->updateClutchPressure(dt / (dt + clutchRC));

            const double brakeRC = keys.brake ? 0.08 : 0.04;
            m_brakePressure += ((keys.brake ? 1.0 : 0.0) - m_brakePressure) * dt / (dt + brakeRC);
            m_sim->getVehicle()->setBrakePressure(m_brakePressure);

            m_sim->startFrame(dt);
            while (m_sim->simulateStep()) { /* run */ }
            m_sim->endFrame();
            if (m_sim->isUnstable()) {
                std::fprintf(stderr, "the simulation became unstable at %d Hz (try a higher simulation_frequency)\n",
                    m_sim->getSimulationFrequency());
                std::exit(4);
            }
            m_sim->readAudioOutput(735, m_audio);
            m_time += dt;
        }

        void run(double seconds, const Keys &keys) {
            const int frames = (int)(seconds * 60.0 + 0.5);
            for (int i = 0; i < frames; ++i) frame(keys);
        }

        void tap(bool up, double throttle) {
            Keys k;
            k.throttle = throttle;
            k.up = up;
            k.down = !up;
            frame(k);
        }

        void print(const char *label) const {
            std::printf("  %-34s t=%5.2fs gear=%2d shifting=%d clutch=%.2f thr=%.2f rpm=%5.0f speed=%5.1f km/h\n",
                label, m_time, m_transmission->getGear() + 1, m_transmission->isShifting() ? 1 : 0,
                m_transmission->getClutchPressure(), m_engine->getSpeedControl(),
                units::toRpm(m_engine->getSpeed()),
                units::convert(m_sim->getVehicle()->getSpeed(), units::km / units::hour));
        }

        double time() const { return m_time; }
        double speed() const { return m_sim->getVehicle()->getSpeed(); }

    private:
        Simulator *m_sim;
        Engine *m_engine;
        Transmission *m_transmission;
        double m_time = 0.0;
        double m_brakePressure = 0.0;
        int16_t m_audio[4096];
    };
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: shift-probe <main.mr> [off]\n");
        return 2;
    }

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

    // One-sample impulse so the synthesizer renders (and drains) without wave files.
    static int16_t impulse[1] = { 32767 };
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        sim->synthesizer().initializeImpulseResponse(impulse, 1, 1.0f, i);
    }
    sim->startAudioRenderingThread();

    std::printf("engine \"%s\"\n", engine->getName().c_str());

    transmission->setSmartShifting(!(argc > 2 && std::string(argv[2]) == "off"));
    std::printf("smart shifting %s\n", transmission->isSmartShifting() ? "on" : "off");

    Driver driver(sim, engine, transmission);
    engine->getIgnitionModule()->m_enabled = true;
    sim->m_starterMotor.m_enabled = true;
    Keys crank;
    crank.throttle = 0.2;
    for (int i = 0; i < 60 * 6 && units::toRpm(engine->getSpeed()) < 600.0; ++i) driver.frame(crank);
    sim->m_starterMotor.m_enabled = false;
    driver.run(1.0, Keys());
    driver.print("idling in neutral");

    Keys full;
    full.throttle = 1.0;

    std::printf("Launch: Up with R held\n");
    driver.tap(true, 1.0);
    for (int i = 0; i < 8; ++i) { driver.run(0.5, full); driver.print("R held"); }

    std::printf("Upshift with R held, then release\n");
    driver.tap(true, 1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.5, full); driver.print("R held"); }
    for (int i = 0; i < 4; ++i) { driver.run(0.5, Keys()); driver.print("no keys"); }

    std::printf("Upshift without throttle, then R\n");
    driver.tap(true, -1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.25, Keys()); driver.print("no keys"); }
    for (int i = 0; i < 4; ++i) { driver.run(0.5, full); driver.print("R held"); }

    std::printf("Two quick upshifts with R held\n");
    driver.tap(true, 1.0);
    driver.run(0.05, full);
    driver.tap(true, 1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.5, full); driver.print("R held"); }
    for (int i = 0; i < 2; ++i) { driver.run(0.5, Keys()); driver.print("no keys"); }

    std::printf("Downshift with R held\n");
    driver.tap(false, 1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.25, full); driver.print("R held"); }

    std::printf("Downshift, then Up right after\n");
    driver.tap(false, -1.0);
    driver.run(0.05, Keys());
    driver.tap(true, -1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.5, Keys()); driver.print("no keys"); }

    std::printf("Clutch tapped for 0.1 s with R held\n");
    Keys clutch = full;
    clutch.clutch = true;
    driver.run(0.1, clutch);
    driver.print("Shift + R held");
    for (int i = 0; i < 8; ++i) { driver.run(1.0 / 60.0, full); driver.print("R held"); }
    for (int i = 0; i < 4; ++i) { driver.run(0.25, full); driver.print("R held"); }

    std::printf("Shift to neutral in the middle of an upshift\n");
    driver.tap(true, 1.0);
    for (int g = 0; g < 8; ++g) driver.tap(false, -1.0);
    for (int i = 0; i < 2; ++i) { driver.run(0.5, Keys()); driver.print("no keys"); }
    driver.tap(true, -1.0);
    for (int i = 0; i < 4; ++i) { driver.run(0.5, full); driver.print("R held"); }

    std::printf("Coast with the clutch in, then brake to a stop\n");
    Keys coast;
    coast.clutch = true;
    double v0 = driver.speed();
    driver.run(1.0, coast);
    driver.print("Shift held");
    std::printf("  coasting: %.3f g\n", (v0 - driver.speed()) / 1.0 / 9.81);
    Keys brake = coast;
    brake.brake = true;
    v0 = driver.speed();
    const double t0 = driver.time();
    double v1 = -1.0;
    for (int i = 0; i < 20 && driver.speed() > 0.1; ++i) {
        driver.run(0.25, brake);
        driver.print("Shift + . held");
        if (driver.time() - t0 > 0.99 && v1 < 0) v1 = driver.speed();
    }
    if (v1 >= 0) std::printf("  braking (first second, incl. pressure build-up): %.3f g\n", (v0 - v1) / 9.81);
    driver.run(1.0, brake);
    driver.print("standstill, Shift + . held");

    sim->endAudioRenderingThread();
    sim->destroy();
    return 0;
}
