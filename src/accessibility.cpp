#include "../include/accessibility.h"

#include "../include/engine_sim_application.h"
#include "../include/constants.h"
#include "../include/units.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
    // An engine counts as running after holding this speed with the starter
    // released for RunningTime, and as stopped below StoppedRpm
    constexpr double RunningRpm = 300.0;
    constexpr double RunningTime = 1.5;
    constexpr double StoppedRpm = 100.0;
    constexpr double StoppedTime = 0.3;

    // Spacing of the dyno report rows
    constexpr double DynoSampleSpacingRpm = 50.0;

    // After the simulation frequency stops changing, wait this long for the
    // load measurement to settle before reporting it
    constexpr double LoadReportDelay = 1.5;

    std::string readFile(const std::string &path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    std::string fileNameSafe(const std::string &name) {
        std::string result;
        for (const char c : name) {
            const bool keep = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
            result += keep ? c : '_';
        }

        return result.empty() ? "engine" : result;
    }

    std::string timestamp() {
        const std::time_t now = std::time(nullptr);
        std::tm local = {};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        std::ostringstream ss;
        ss << std::put_time(&local, "%Y%m%d-%H%M%S");
        return ss.str();
    }
}

Accessibility::Accessibility() {
    m_app = nullptr;
    m_forced = false;
    m_muted = false;
    m_time = 0.0;
    m_lastAnnouncementTime = -1.0;
    m_running = false;
    m_runningTimer = 0.0;
    m_stoppedTimer = 0.0;
    m_fuelSampleTime = 0.0;
    m_fuelSample = 0.0;
    m_fuelRate = 0.0;
    m_dynoSweeping = false;
    m_dynoTorque = 0.0;
    m_dynoPower = 0.0;
    m_lastSimulationFrequency = 0.0;
    m_loadReportTimer = 0.0;
}

Accessibility::~Accessibility() {
    /* void */
}

void Accessibility::initialize(EngineSimApplication *app) {
    m_app = app;
    m_screenReader.initialize();

    const char *logPath = std::getenv("ENGINE_SIM_SPEECH_LOG");
    if (logPath != nullptr && *logPath != '\0') {
        m_log.open(logPath, std::ios::out | std::ios::app);

        // Tells from a user's log whether their screen reader was found
        const std::string backend = m_screenReader.getBackendName();
        m_log << "# speech support " << (ScreenReader::isSupported() ? "built in" : "not built in")
            << ", screen reader: " << (backend.empty() ? "none found" : backend) << std::endl;
    }
}

void Accessibility::destroy() {
    m_screenReader.destroy();
    m_log.close();
}

bool Accessibility::isActive() {
    if (m_muted) return false;
    return m_log.is_open() || m_forced || m_screenReader.isScreenReaderRunning();
}

void Accessibility::announce(const std::string &text, bool queue) {
    if (text.empty() || !isActive()) return;

    // Several announcements in one frame queue up instead of cutting each
    // other off
    const bool interrupt = !queue && (m_time != m_lastAnnouncementTime);
    m_lastAnnouncementTime = m_time;
    m_lastAnnouncement = text;

    if (m_log.is_open()) {
        m_log << std::fixed << std::setprecision(2) << m_time << "\t" << text << std::endl;
    }

    m_screenReader.output(text, interrupt, m_forced);
}

void Accessibility::event(const std::string &text, bool queue) {
    if (m_announcer.announcesEvents()) {
        announce(text, queue);
    }
}

void Accessibility::update(float dt) {
    m_time += dt;

    processKeys();
    detectEngineState(dt);
    sampleFuel();
    recordDynoRun(dt);
    watchSimulationFrequency(dt);

    announce(m_announcer.poll(m_time));
}

void Accessibility::onLogMessage(const std::string &message) {
    announce(m_announcer.onLogMessage(message, m_time));
}

void Accessibility::onScriptLoaded() {
    m_running = false;
    m_runningTimer = 0.0;
    m_stoppedTimer = 0.0;
    m_fuelSample = 0.0;
    m_fuelRate = 0.0;
    m_dynoRun.clear();
    m_dynoSweeping = false;

    // The engine sets its own frequency: not a change worth reporting
    m_lastSimulationFrequency = m_app->getSimulator()->getSimulationFrequency();
    m_loadReportTimer = 0.0;

    Engine *engine = m_app->getSimulator()->getEngine();
    if (engine != nullptr) {
        event("Loaded " + engine->getName());
    }
}

void Accessibility::onScriptFailed(const std::string &errorLogPath) {
    const std::string error = spoken_text::scriptError(readFile(errorLogPath));
    event(error.empty() ? "The script failed to load" : error);
}

void Accessibility::onDynoRunFinished() {
    std::string text = spoken_text::dynoSummary(m_dynoRun, units());
    if (text.empty()) return;

    std::string fileName;
    if (isActive() && writeDynoReport(&fileName)) {
        text += ". Report saved as " + fileName;
    }

    event(text);
    m_dynoRun.clear();
}

bool Accessibility::writeDynoReport(std::string *fileName) {
    Engine *engine = m_app->getSimulator()->getEngine();
    const std::string name = "dyno_"
        + fileNameSafe(engine != nullptr ? engine->getName() : "") + "_" + timestamp() + ".csv";

    std::ofstream file(m_app->getOutputPath().Append(name).ToString(), std::ios::out);
    if (!file.is_open()) return false;

    const spoken_text::Units u = units();
    const bool newtonMeters = (u.torque == "Nm");
    const bool kilowatts = (u.power == "kW");

    file << "rpm," << (newtonMeters ? "torque_nm" : "torque_lb_ft")
        << "," << (kilowatts ? "power_kw" : "power_hp") << "\n";
    file << std::fixed << std::setprecision(1);
    double lastRpm = -DynoSampleSpacingRpm;
    for (const spoken_text::DynoPoint &sample : m_dynoRun) {
        if (sample.rpm < lastRpm + DynoSampleSpacingRpm) continue;
        lastRpm = sample.rpm;

        file << sample.rpm << ","
            << (newtonMeters ? units::convert(sample.torque, units::Nm) : units::convert(sample.torque, units::ft_lb)) << ","
            << (kilowatts ? units::convert(sample.power, units::kW) : units::convert(sample.power, units::hp)) << "\n";
    }

    *fileName = name;
    return static_cast<bool>(file);
}

void Accessibility::processKeys() {
    dbasic::DeltaEngine *keys = m_app->getEngine();

    if (keys->ProcessKeyDown(ysKey::Code::J)) {
        const spoken_text::SpeechSetting next =
            spoken_text::speechKey(isActive(), m_announcer.getVerbosity());

        if (next.on) {
            // Also turns speech on without a screen reader (system voice)
            m_muted = false;
            m_forced = true;
            m_announcer.setVerbosity(next.verbosity);
            announce(next.announcement);
        }
        else {
            // Said while still speaking, so the user knows what happened
            announce(next.announcement);
            m_announcer.setVerbosity(next.verbosity);
            m_forced = false;
            m_muted = true;
        }
    }

    if (keys->ProcessKeyDown(ysKey::Code::F4)) {
        announce("Opening the accessibility guide in your web browser");
        openGuide();
    }

    // Nothing to read out to: keep the keys free of side effects
    if (!isActive()) return;

    const spoken_text::Units u = units();
    if (keys->ProcessKeyDown(ysKey::Code::F5)) announce(spoken_text::engineReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F6)) announce(spoken_text::vehicleReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F7)) announce(spoken_text::dynoReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F8)) announce(spoken_text::airReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F9)) announce(spoken_text::fuelReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F11)) announce(spoken_text::infoReadout(read(), u));
    if (keys->ProcessKeyDown(ysKey::Code::F12)) announce(spoken_text::audioReadout(read(), u));

    if (keys->ProcessKeyDown(ysKey::Code::Home)) {
        announce(m_lastAnnouncement.empty() ? "Nothing announced yet" : m_lastAnnouncement);
    }
}

void Accessibility::detectEngineState(float dt) {
    Simulator *simulator = m_app->getSimulator();
    Engine *engine = (simulator != nullptr) ? simulator->getEngine() : nullptr;
    if (engine == nullptr) return;

    const double rpm = std::abs(engine->getRpm());
    const bool ignition = engine->getIgnitionModule()->m_enabled;
    const bool starter = simulator->m_starterMotor.m_enabled;

    if (!m_running) {
        if (ignition && !starter && rpm > RunningRpm) {
            m_runningTimer += dt;
            if (m_runningTimer >= RunningTime) {
                m_running = true;
                m_stoppedTimer = 0.0;
                event("Engine running");
            }
        }
        else {
            m_runningTimer = 0.0;
        }
    }
    else {
        if (rpm < StoppedRpm) {
            m_stoppedTimer += dt;
            if (m_stoppedTimer >= StoppedTime) {
                m_running = false;
                m_runningTimer = 0.0;
                event(ignition ? "Engine stalled" : "Engine stopped");
            }
        }
        else {
            m_stoppedTimer = 0.0;
        }
    }
}

void Accessibility::sampleFuel() {
    Engine *engine = m_app->getSimulator()->getEngine();
    if (engine == nullptr || m_time - m_fuelSampleTime < 1.0) return;

    const double consumed = engine->getTotalVolumeFuelConsumed();
    const double rate = (consumed - m_fuelSample) / (m_time - m_fuelSampleTime);
    m_fuelRate = (rate > 0) ? rate : 0.0;
    m_fuelSample = consumed;
    m_fuelSampleTime = m_time;
}

void Accessibility::recordDynoRun(float dt) {
    Simulator *simulator = m_app->getSimulator();
    Engine *engine = simulator->getEngine();

    const bool sweeping = engine != nullptr
        && simulator->m_dyno.m_enabled
        && !simulator->m_dyno.m_hold;
    if (sweeping && !m_dynoSweeping) {
        m_dynoRun.clear();
        m_dynoTorque = simulator->getFilteredDynoTorque();
        m_dynoPower = simulator->getDynoPower();
    }

    m_dynoSweeping = sweeping;
    if (!sweeping) return;

    // Smoothed like the dyno panel (LoadSimulationCluster), so the peaks
    // agree with what it and the I key report
    constexpr double RC = 0.1;
    const double alpha = dt / (dt + RC);
    m_dynoTorque += alpha * (simulator->getFilteredDynoTorque() - m_dynoTorque);
    m_dynoPower += alpha * (simulator->getDynoPower() - m_dynoPower);

    m_dynoRun.push_back({ std::abs(engine->getRpm()), m_dynoTorque, m_dynoPower });
}

void Accessibility::watchSimulationFrequency(float dt) {
    const double frequency = m_app->getSimulator()->getSimulationFrequency();
    if (frequency != m_lastSimulationFrequency) {
        // Changed with N: tell whether the computer keeps up once the
        // frequency has stayed put long enough to measure it
        if (m_lastSimulationFrequency > 0) m_loadReportTimer = LoadReportDelay;
        m_lastSimulationFrequency = frequency;
        return;
    }

    if (m_loadReportTimer <= 0) return;
    m_loadReportTimer -= dt;
    if (m_loadReportTimer > 0) return;

    // Queued behind the new frequency if that is still being spoken
    const PerformanceCluster *performance = m_app->getPerformanceCluster();
    if (performance != nullptr) {
        event(spoken_text::simulationLoad(performance->getSimulationLoad()), true);
    }
}

void Accessibility::openGuide() {
    // The data root can be relative to the working directory
    std::error_code ec;
    const std::string guide = std::filesystem::absolute(
        m_app->getDataRoot().Append("docs/accessibility.html").ToString(), ec).string();

#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, 0, guide.c_str(), -1, nullptr, 0);
    std::wstring wide(length > 0 ? length : 0, L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_UTF8, 0, guide.c_str(), -1, wide.data(), length);
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
#elif defined(__APPLE__)
    std::system(("open \"" + guide + "\" &").c_str());
#else
    std::system(("xdg-open \"" + guide + "\" >/dev/null 2>&1 &").c_str());
#endif
}

spoken_text::Units Accessibility::units() {
    const ApplicationSettings *settings = m_app->getAppSettings();

    spoken_text::Units u;
    u.power = settings->powerUnits;
    u.torque = settings->torqueUnits;
    u.speed = settings->speedUnits;
    u.pressure = settings->pressureUnits;
    return u;
}

spoken_text::Readings Accessibility::read() {
    spoken_text::Readings r;

    Simulator *simulator = m_app->getSimulator();
    Engine *engine = simulator->getEngine();

    if (engine != nullptr) {
        r.hasEngine = true;
        r.engineName = engine->getName();
        r.cylinders = engine->getCylinderCount();
        r.displacement = engine->getDisplacement();
        r.redline = engine->getRedline();
        r.engineSpeed = units::rpm(engine->getRpm());
        r.throttle = engine->getSpeedControl();
        r.ignition = engine->getIgnitionModule()->m_enabled;
        r.starter = simulator->m_starterMotor.m_enabled;

        r.manifoldPressure = engine->getManifoldPressure();
        r.airFlow = engine->getIntakeFlowRate();
        r.airFuelRatio = engine->getIntakeAfr();

        // Same as the volumetric efficiency gauge (RightGaugeCluster)
        constexpr double ambientPressure = units::pressure(1.0, units::atm);
        constexpr double ambientTemperature = units::celcius(25.0);
        const double airPerRevolution =
            0.5 * (ambientPressure * engine->getDisplacement()) / (constants::R * ambientTemperature);
        const double theoreticalAirPerSecond = airPerRevolution * std::fmax(engine->getRpm(), 0.0) / 60.0;
        r.volumetricEfficiency = (std::abs(theoreticalAirPerSecond) < 1E-3)
            ? 0.0
            : r.airFlow / theoreticalAirPerSecond;

        for (int i = 0; i < engine->getCompressorCount(); ++i) {
            const double boost = engine->getCompressor(i)->getBoost();
            if (!r.hasCompressor || boost > r.boost) r.boost = boost;
            r.hasCompressor = true;
        }

        r.fuelConsumed = engine->getTotalVolumeFuelConsumed();
        r.fuelRate = m_fuelRate;
    }

    Vehicle *vehicle = simulator->getVehicle();
    if (vehicle != nullptr) {
        r.vehicleSpeed = vehicle->getSpeed();
        r.distance = vehicle->getTravelledDistance();
        r.brake = vehicle->getBrakePressure();
    }

    Transmission *transmission = simulator->getTransmission();
    if (transmission != nullptr) {
        r.gear = transmission->getGear();
        r.clutch = transmission->getClutchPressure();
    }

    r.dynoEnabled = simulator->m_dyno.m_enabled;
    r.dynoHold = simulator->m_dyno.m_hold;
    r.dynoSpeed = simulator->m_dyno.m_rotationSpeed;
    r.torque = simulator->getFilteredDynoTorque();
    r.power = simulator->getDynoPower();

    const Synthesizer::AudioParameters audio = simulator->synthesizer().getAudioParameters();
    r.volume = audio.volume;
    r.convolution = audio.convolution;
    switch (audio.levelerMode) {
        case LevelingFilter::Mode::Loudness: r.leveler = "loudness"; break;
        case LevelingFilter::Mode::Peak: r.leveler = "original"; break;
        default: r.leveler = "off"; break;
    }

    r.simulationFrequency = simulator->getSimulationFrequency();
    if (m_app->getPerformanceCluster() != nullptr) {
        r.frameRate = m_app->getEngine()->GetAverageFramerate();
        r.simulationLoad = m_app->getPerformanceCluster()->getSimulationLoad();
    }

    return r;
}
