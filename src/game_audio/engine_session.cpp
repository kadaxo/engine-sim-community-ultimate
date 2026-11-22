#include "../../include/game_audio/engine_session.h"

#include "../../include/game_audio/wav_file.h"

#include "../../include/engine.h"
#include "../../include/exhaust_system.h"
#include "../../include/impulse_response.h"
#include "../../include/simulator.h"
#include "../../include/transmission.h"
#include "../../include/units.h"
#include "../../include/vehicle.h"

#include "../../include/engine_script.h"
#include "../../scripting/include/compiler.h"

#include <cstdint>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>

namespace {

    // The piranha-based script compiler stores its output in a process-wide
    // singleton, so compilation must not run concurrently.
    std::mutex &compilerMutex() {
        static std::mutex m;
        return m;
    }

    // Compiles and runs a script. Must be called with compilerMutex() held.
    bool runScript(
        const std::string &path,
        const std::vector<piranha::IrPath> &searchPaths,
        es_script::Compiler::Output *output,
        std::string *error)
    {
        // initialize() also clears the previous compile's output, so nothing
        // leaks into this one if the script omits a node.
        es_script::Compiler compiler;
        compiler.initialize(searchPaths);

        std::stringstream log;
        if (!compiler.compile(path.c_str(), log)) {
            compiler.destroy();

            *error = "failed to compile '" + path + "'"
                + (log.str().empty() ? std::string() : ":\n" + log.str());
            return false;
        }

        *output = compiler.execute();
        compiler.destroy();
        return true;
    }

    std::string readTextFile(const std::string &path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    // Time constant of the throttle response. The interactive application
    // moves the speed control halfway to its target every frame (~60 Hz),
    // which corresponds to roughly 24 ms.
    constexpr double ThrottleTimeConstant = 0.024;

} /* namespace */

game_audio::EngineSession::~EngineSession() {
    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
    }

    if (m_engine != nullptr) {
        m_engine->destroy();
        delete m_engine;
    }

    delete m_vehicle;
    delete m_transmission;
}

std::unique_ptr<game_audio::EngineSession> game_audio::EngineSession::create(
    const SessionConfig &config, std::string *error)
{
    std::unique_ptr<EngineSession> session(new EngineSession);

    {
        std::lock_guard<std::mutex> lock(compilerMutex());

        std::vector<piranha::IrPath> searchPaths;
        for (const std::string &path : config.searchPaths) {
            searchPaths.push_back(path.c_str());
        }

        es_script::Compiler::Output output;
        if (!runScript(config.scriptPath, searchPaths, &output, error)) {
            return nullptr;
        }

        // Engine files from assets/engines only define nodes. Run them the way
        // LOAD ENGINE does, from a script that imports the file and calls it.
        const std::string call = (output.engine == nullptr)
            ? engineScriptCall(readTextFile(config.scriptPath))
            : std::string();
        if (!call.empty()) {
            namespace fs = std::filesystem;

            std::error_code ec;
            const fs::path engineFile = fs::weakly_canonical(fs::absolute(config.scriptPath, ec), ec);
            const fs::path wrapper = fs::temp_directory_path(ec)
                / ("engine-sim-export-" + std::to_string(std::random_device()()) + ".mr");

            {
                std::ofstream script(wrapper, std::ios::out);
                script << "import \"engine_sim.mr\"\n";
                script << "import \"" << engineFile.generic_string() << "\"\n\n";
                script << call << "\n";
                if (!script) {
                    *error = "can't write '" + wrapper.string() + "'";
                    return nullptr;
                }
            }

            std::string wrapperError;
            const bool ran = runScript(wrapper.generic_string(), searchPaths, &output, &wrapperError);
            fs::remove(wrapper, ec);
            if (!ran) {
                *error = "'" + config.scriptPath + "' doesn't call set_engine(), and running it as an engine file ("
                    + call + ") failed:\n" + wrapperError;
                return nullptr;
            }
        }

        session->m_engine = output.engine;
        session->m_vehicle = output.vehicle;
        session->m_transmission = output.transmission;
    }

    if (session->m_engine == nullptr) {
        *error = "'" + config.scriptPath + "' doesn't define an engine (it neither calls set_engine() nor defines a public engine node)";
        return nullptr;
    }

    // Same fallbacks as the interactive application.
    if (session->m_vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        session->m_vehicle = new Vehicle;
        session->m_vehicle->initialize(vehParams);
    }

    if (session->m_transmission == nullptr) {
        const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        session->m_transmission = new Transmission;
        session->m_transmission->initialize(tParams);
    }

    Engine *engine = session->m_engine;
    session->m_simulator = engine->createSimulator(session->m_vehicle, session->m_transmission);
    engine->calculateDisplacement();

    Simulator *simulator = session->m_simulator;
    simulator->setSimulationFrequency(static_cast<int>(std::lround(
        config.simulationFrequency > 0
            ? config.simulationFrequency
            : engine->getSimulationFrequency())));

    // Offline rendering consumes every input sample right away, so there is
    // no latency to regulate; a zero target keeps the step count per block
    // exactly proportional to the block duration.
    simulator->setTargetSynthesizerLatency(0.0);
    simulator->setSimulationSpeed(1.0);

    Synthesizer::AudioParameters audioParams = simulator->synthesizer().getAudioParameters();
    audioParams.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.airNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    audioParams.listenerPosition = config.listenerPosition;
    audioParams.engineLocation = engine->getLocation();
    simulator->synthesizer().setAudioParameters(audioParams);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        const ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();
        if (response == nullptr) {
            *error = "exhaust system " + std::to_string(i) + " has no impulse response";
            return nullptr;
        }

        std::vector<int16_t> ir;
        int irSampleRate = 0;
        std::string wavError;
        if (!readWavPcm16Mono(response->getFilename(), &ir, &irSampleRate, &wavError)) {
            *error = "impulse response: " + wavError;
            return nullptr;
        }

        if (irSampleRate != AudioSampleRate) {
            *error = "impulse response '" + response->getFilename() + "' is "
                + std::to_string(irSampleRate) + " Hz; engine-sim requires "
                + std::to_string(AudioSampleRate) + " Hz";
            return nullptr;
        }

        simulator->synthesizer().initializeImpulseResponse(
            ir.data(),
            static_cast<unsigned int>(ir.size()),
            static_cast<float>(response->getVolume()),
            i);
    }

    // Neutral, clutch engaged: the dyno and starter act directly on the crank.
    // A new transmission starts in neutral.
    assert(session->m_transmission->getGear() == -1);
    session->m_transmission->setClutchPressure(1.0);

    if (config.starterRpm > 0) {
        // Keep the script's direction: a counter-clockwise engine's starter
        // turns the other way
        const double direction = (simulator->m_starterMotor.m_rotationSpeed > 0) ? -1.0 : 1.0;
        simulator->m_starterMotor.m_rotationSpeed = -direction * units::rpm(config.starterRpm);
    }
    session->m_crankThrottle = std::clamp(config.crankThrottle, 0.0, 1.0);

    Info &info = session->m_info;
    info.name = engine->getName();
    info.cylinderCount = engine->getCylinderCount();
    info.exhaustSystemCount = engine->getExhaustSystemCount();
    info.displacementLiters = engine->getDisplacement() * 1000.0;
    info.redlineRpm = units::toRpm(engine->getRedline());
    info.dynoMinRpm = units::toRpm(engine->getDynoMinSpeed());
    info.dynoMaxRpm = units::toRpm(engine->getDynoMaxSpeed());
    info.starterRpm = units::toRpm(std::abs(simulator->m_starterMotor.m_rotationSpeed));
    info.simulationFrequency = simulator->getSimulationFrequency();

    return session;
}

void game_audio::EngineSession::setThrottle(double target) {
    m_targetThrottle = std::max(0.0, std::min(1.0, target));
}

void game_audio::EngineSession::setThrottleImmediate(double value) {
    m_targetThrottle = m_throttle = std::max(0.0, std::min(1.0, value));
    m_engine->setSpeedControl(m_throttle);
}

void game_audio::EngineSession::setIgnition(bool enabled) {
    m_engine->getIgnitionModule()->m_enabled = enabled;
}

void game_audio::EngineSession::setStarter(bool enabled) {
    m_simulator->m_starterMotor.m_enabled = enabled;
}

void game_audio::EngineSession::setDynoHold(double rpm) {
    m_simulator->m_dyno.m_enabled = true;
    m_simulator->m_dyno.m_hold = true;
    m_simulator->m_dyno.m_rotationSpeed = units::rpm(std::max(0.0, rpm));
}

void game_audio::EngineSession::releaseDyno() {
    m_simulator->m_dyno.m_enabled = false;
    m_simulator->m_dyno.m_hold = false;
    m_simulator->m_dyno.m_rotationSpeed = 0.0;
}

double game_audio::EngineSession::rpm() const {
    return m_engine->getRpm();
}

void game_audio::EngineSession::advance(
    double seconds,
    std::vector<float> *audio,
    std::vector<TelemetrySample> *telemetry)
{
    // Stop once less than half a physics step remains; such a remainder
    // would round to zero steps and never advance time.
    const double end = m_time + seconds;
    const double minimum = 0.5 * m_simulator->getTimestep();
    while (end - m_time > minimum) {
        advanceBlock(std::min(BlockDuration, end - m_time), audio, telemetry);
    }
}

bool game_audio::EngineSession::advanceUntil(
    const std::function<bool(const EngineSession &)> &condition,
    double timeout,
    std::vector<float> *audio,
    std::vector<TelemetrySample> *telemetry)
{
    const double end = m_time + timeout;
    const double minimum = 0.5 * m_simulator->getTimestep();
    while (end - m_time > minimum) {
        advanceBlock(std::min(BlockDuration, end - m_time), audio, telemetry);
        if (condition(*this)) return true;
    }

    return false;
}

void game_audio::EngineSession::advanceBlock(
    double dt,
    std::vector<float> *audio,
    std::vector<TelemetrySample> *telemetry)
{
    const double alpha = 1.0 - std::exp(-dt / ThrottleTimeConstant);
    m_throttle += (m_targetThrottle - m_throttle) * alpha;
    m_engine->setSpeedControl(m_throttle);

    m_simulator->startFrame(dt);
    const int steps = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        /* void */
    }
    m_simulator->endFrame();

    std::vector<float> &target = (audio != nullptr) ? *audio : m_scratch;
    if (audio == nullptr) m_scratch.clear();
    m_simulator->synthesizer().renderOffline(target);

    m_time += steps * m_simulator->getTimestep();

    if (telemetry != nullptr) {
        TelemetrySample sample;
        sample.time = m_time;
        sample.audioSample = (audio != nullptr) ? audio->size() : 0;
        sample.rpm = m_engine->getRpm();
        sample.throttle = m_throttle;
        sample.dynoTorque = m_simulator->m_dyno.m_enabled
            ? m_simulator->m_dyno.getTorque()
            : 0.0;
        telemetry->push_back(sample);
    }
}
