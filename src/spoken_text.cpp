#include "../include/spoken_text.h"

#include "../include/units.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

namespace {
    constexpr double AmbientPressure = units::pressure(1.0, units::atm);

    bool imperial(const spoken_text::Units &units) {
        return units.speed == "mph";
    }

    // "IGNITION ENABLED" -> "Ignition enabled"; mixed-case text is kept
    std::string sentenceCase(const std::string &text) {
        for (const char c : text) {
            if (std::islower(static_cast<unsigned char>(c))) return text;
        }

        std::string result = text;
        bool first = true;
        for (char &c : result) {
            if (!std::isalpha(static_cast<unsigned char>(c))) continue;
            c = first
                ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            first = false;
        }

        return result;
    }

    // "1 mile", "2 miles"
    std::string count(const std::string &number, const char *singular, const char *plural) {
        return number + " " + ((number == "1") ? singular : plural);
    }

    void join(std::string &text, const std::string &part) {
        if (part.empty()) return;
        if (!text.empty()) text += ", ";
        text += part;
    }

    // "load 45 percent". The step time excludes drawing, so warn a little
    // before the simulation falls behind and the sound breaks up.
    std::string load(double load) {
        const double percent = std::round(100.0 * load);
        std::string text = "load " + spoken_text::number(percent) + " percent";
        if (percent > 100) text += ", too high for this computer";
        else if (percent >= 80) text += ", close to the limit";
        return text;
    }

    struct Adjustment {
        const char *logName;
        const char *spokenName;
        const char *unit;
        int decimals;
        spoken_text::Category category;
    };

    // Values set with a key held and the mouse wheel (or PageUp/PageDown)
    const Adjustment Adjustments[] = {
        { "volume", "Volume", "", 2, spoken_text::Category::Adjustment },
        { "convolution level", "Convolution", "", 2, spoken_text::Category::Adjustment },
        { "high freq. gain", "High frequency gain", "", 4, spoken_text::Category::Adjustment },
        { "low freq. noise", "Low frequency noise", "", 3, spoken_text::Category::Adjustment },
        { "high freq. noise", "High frequency noise", "", 3, spoken_text::Category::Adjustment },
        { "simulation freq", "Simulation frequency", " hertz", 0, spoken_text::Category::Adjustment },
        { "dyno speed", "Dyno speed", " rpm", 0, spoken_text::Category::Adjustment },
        { "render layer", "View layer", "", 0, spoken_text::Category::View }
    };

    struct Rewording {
        const char *logMessage;
        const char *spoken;
    };

    const Rewording Rewordings[] = {
        { "IGNITION ENABLED", "Ignition on" },
        { "IGNITION DISABLED", "Ignition off" },
        { "STARTER ENABLED", "Starter on" },
        { "STARTER DISABLED", "Starter off" },
        { "DYNOMOMETER ENABLED", "Dyno on" },
        { "DYNOMOMETER DISABLED", "Dyno off" },
        { "HOLD ENABLED", "Hold on" },
        { "HOLD DISABLED", "Hold off" },
        { "HOLD ON STANDBY [ENABLE DYNO. FOR HOLD]", "Hold on standby, turn the dyno on to hold" },
        { "SHIFTED TO NEUTRAL", "Neutral" },
        { "CLUTCH DEPRESSED", "Clutch in" },
        { "LEVELER: LOUDNESS", "Leveler loudness" },
        { "LEVELER: ORIGINAL (PEAK)", "Leveler original" },
        { "LEVELER: OFF", "Leveler off" },
        { "KEY LIST SHOWN", "Key list shown on screen. The accessibility guide, F4, has every key" },
        { "KEY LIST CLOSED", "Key list closed" }
    };
}

spoken_text::SpeechSetting spoken_text::speechKey(bool speaking, Verbosity current) {
    SpeechSetting next;
    if (!speaking) {
        next.on = true;
        next.verbosity = Verbosity::Normal;
        next.announcement = "Speech on. Announcements: normal";
        return next;
    }

    next.on = true;
    switch (current) {
        case Verbosity::Normal:
            next.verbosity = Verbosity::All;
            next.announcement = "Announcements: all, including throttle";
            break;
        case Verbosity::All:
            next.verbosity = Verbosity::ReadoutsOnly;
            next.announcement = "Announcements: readout keys only";
            break;
        default:
            next.on = false;
            next.verbosity = Verbosity::Normal;
            next.announcement = "Speech off. Press J to turn it back on";
            break;
    }
    return next;
}

std::string spoken_text::number(double value, int decimals) {
    if (!std::isfinite(value)) return "0";

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals > 0 ? decimals : 0) << value;

    std::string s = ss.str();
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }

    if (s == "-0") s = "0";
    return s;
}

spoken_text::Message spoken_text::fromLogMessage(const std::string &logMessage) {
    Message message;
    message.category = Category::Event;

    static const std::regex adjustment(R"(^\[.\] - Set (.+) to (\S+)$)");
    static const std::regex throttle(R"(^Speed control set to (\S+)$)");
    static const std::regex gear(R"(^(UP|DOWN)SHIFTED TO (\d+)$)");
    static const std::regex peaks(R"(^(\d+)(hp|kW) @ (\d+)rpm \| (\d+)(lb-ft|Nm) @ (\d+)rpm$)");

    std::smatch match;
    if (std::regex_match(logMessage, match, adjustment)) {
        for (const Adjustment &a : Adjustments) {
            if (match[1] == a.logName) {
                message.text = std::string(a.spokenName) + " "
                    + number(std::atof(match[2].str().c_str()), a.decimals) + a.unit;
                message.category = a.category;
                return message;
            }
        }
    }
    else if (std::regex_match(logMessage, match, throttle)) {
        message.text = "Throttle " + number(100.0 * std::atof(match[1].str().c_str()), 1) + " percent";
        message.category = Category::Throttle;
        return message;
    }
    else if (std::regex_match(logMessage, match, gear)) {
        message.text = "Gear " + match[2].str();
        return message;
    }
    else if (std::regex_match(logMessage, match, peaks)) {
        message.text = "Peak power " + match[1].str()
            + (match[2] == "hp" ? " horsepower" : " kilowatts")
            + " at " + match[3].str() + " rpm, peak torque " + match[4].str()
            + (match[5] == "Nm" ? " newton meters" : " pound-feet")
            + " at " + match[6].str() + " rpm";
        return message;
    }
    else if (logMessage.rfind("Script failed", 0) == 0) {
        // The error itself is read from the log, see scriptError()
        message.category = Category::Silent;
        return message;
    }
    else if (logMessage.rfind("RPM sync", 0) == 0) {
        // Status for tools that drive the simulator over the network, shown
        // on every start: not worth hearing an IP address read out each time
        message.category = Category::Silent;
        return message;
    }

    for (const Rewording &r : Rewordings) {
        if (logMessage == r.logMessage) {
            message.text = r.spoken;
            return message;
        }
    }

    message.text = sentenceCase(logMessage);
    return message;
}

std::string spoken_text::Announcer::onLogMessage(const std::string &logMessage, double time) {
    if (logMessage == m_lastMessage && time - m_lastMessageTime < RepeatWindow) {
        m_lastMessageTime = time;
        return "";
    }

    m_lastMessage = logMessage;
    m_lastMessageTime = time;

    if (m_verbosity == Verbosity::ReadoutsOnly) return "";

    const Message message = fromLogMessage(logMessage);
    switch (message.category) {
        case Category::Event:
            return message.text;
        case Category::Throttle:
            if (m_verbosity != Verbosity::All) return "";
            // Fine throttle control changes it continuously
            m_pending = message.text;
            m_pendingTime = time;
            return "";
        case Category::Adjustment:
            m_pending = message.text;
            m_pendingTime = time;
            return "";
        case Category::View:
            return (m_verbosity == Verbosity::All) ? message.text : "";
        default:
            return "";
    }
}

std::string spoken_text::Announcer::poll(double time) {
    if (m_pending.empty() || time - m_pendingTime < SettleTime) return "";

    std::string text;
    text.swap(m_pending);
    return announcesEvents() ? text : "";
}

std::string spoken_text::scriptError(const std::string &errorLog) {
    static const std::regex compileError(R"(^(.+)\((\d+)\): error \S+: (.*)$)");
    static const std::regex missingFile(R"(^Can't find file: (.*)$)");

    std::stringstream lines(errorLog);
    std::string line;
    std::string first;
    std::string firstOther;
    int errors = 0;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::smatch match;
        if (std::regex_match(line, match, compileError)) {
            if (errors++ == 0) {
                first = "Script error in " + match[1].str() + " line " + match[2].str()
                    + ": " + match[3].str();
            }
        }
        else if (std::regex_match(line, match, missingFile)) {
            if (errors++ == 0) {
                first = "Script file not found: " + match[1].str();
            }
        }
        else if (firstOther.empty() && line.find_first_not_of(" \t") != std::string::npos) {
            firstOther = line;
        }
    }

    if (errors == 0) {
        return firstOther.empty() ? std::string() : "Script error: " + firstOther;
    }
    else if (errors > 1) {
        first += ". " + std::to_string(errors) + " errors in total";
    }

    return first;
}

std::string spoken_text::dynoSummary(const std::vector<DynoPoint> &run, const Units &units) {
    if (run.size() < 2) return "";

    const DynoPoint *peakPower = &run.front();
    const DynoPoint *peakTorque = &run.front();
    for (const DynoPoint &point : run) {
        if (point.power > peakPower->power) peakPower = &point;
        if (point.torque > peakTorque->torque) peakTorque = &point;
    }

    return "Dyno run finished. Peak power " + power(peakPower->power, units)
        + " at " + number(peakPower->rpm) + " rpm, peak torque "
        + torque(peakTorque->torque, units) + " at " + number(peakTorque->rpm) + " rpm";
}

std::string spoken_text::engineSpeed(double angularSpeed) {
    return number(std::abs(units::toRpm(angularSpeed))) + " rpm";
}

std::string spoken_text::torque(double torque, const Units &units) {
    return (units.torque == "Nm")
        ? number(units::convert(torque, units::Nm)) + " newton meters"
        : number(units::convert(torque, units::ft_lb)) + " pound-feet";
}

std::string spoken_text::power(double power, const Units &units) {
    return (units.power == "kW")
        ? number(units::convert(power, units::kW)) + " kilowatts"
        : number(units::convert(power, units::hp)) + " horsepower";
}

std::string spoken_text::engineReadout(const Readings &r, const Units &) {
    if (!r.hasEngine) return "No engine loaded";

    std::string text = engineSpeed(r.engineSpeed);
    join(text, "throttle " + number(100.0 * r.throttle, 1) + " percent");
    join(text, r.ignition ? "ignition on" : "ignition off");
    if (r.starter) join(text, "starter on");

    return text;
}

std::string spoken_text::vehicleReadout(const Readings &r, const Units &units) {
    std::string text = imperial(units)
        ? count(number(units::convert(std::abs(r.vehicleSpeed), units::mile / units::hour)),
            "mile per hour", "miles per hour")
        : count(number(units::convert(std::abs(r.vehicleSpeed), units::km / units::hour)),
            "kilometer per hour", "kilometers per hour");

    join(text, (r.gear < 0) ? "neutral" : "gear " + std::to_string(r.gear + 1));

    if (r.clutch >= 0.99) join(text, "clutch engaged");
    else if (r.clutch <= 0.01) join(text, "clutch disengaged");
    else join(text, "clutch " + number(100.0 * r.clutch) + " percent engaged");

    if (r.brake > 0.01) join(text, "braking");

    return text;
}

std::string spoken_text::dynoReadout(const Readings &r, const Units &units) {
    if (!r.dynoEnabled) {
        return r.dynoHold ? "Dyno off, hold on standby" : "Dyno off";
    }

    std::string text = r.dynoHold
        ? "Dyno holding " + engineSpeed(r.dynoSpeed)
        : "Dyno sweep at " + engineSpeed(r.engineSpeed);
    join(text, torque(r.torque, units));
    join(text, power(r.power, units));

    return text;
}

std::string spoken_text::airReadout(const Readings &r, const Units &units) {
    std::string text;

    const double relative = r.manifoldPressure - AmbientPressure;
    if (units.pressure == "kPa") {
        text = "Manifold pressure " + number(units::convert(r.manifoldPressure, units::kPa)) + " kilopascals";
    }
    else if (units.pressure == "mbar") {
        text = "Manifold pressure " + number(units::convert(r.manifoldPressure, units::mbar)) + " millibar";
    }
    else if (units.pressure == "bar") {
        text = "Manifold pressure " + number(units::convert(r.manifoldPressure, units::bar), 2) + " bar";
    }
    else {
        const bool psi = (units.pressure == "psi");
        const double value = psi
            ? units::convert(relative, units::psi)
            : units::convert(relative, units::inHg);
        const std::string unit = psi ? " psi" : " inches of mercury";

        if (std::abs(value) < 0.5) text = "Manifold at atmospheric pressure";
        else if (value < 0) text = "Manifold vacuum " + number(-value, 1) + unit;
        else text = "Manifold boost " + number(value, 1) + unit;
    }

    join(text, "air flow " + number(units::convert(r.airFlow, units::scfm)) + " SCFM");
    join(text, "volumetric efficiency " + number(100.0 * r.volumetricEfficiency) + " percent");
    if (r.airFuelRatio > 0) {
        join(text, "air fuel ratio " + number(r.airFuelRatio, 1));
    }

    if (r.hasCompressor) {
        const bool psi = (units.pressure == "inHg" || units.pressure == "psi");
        join(text, psi
            ? "boost " + number(units::convert(r.boost, units::psi), 1) + " psi"
            : "boost " + number(units::convert(r.boost, units::bar), 2) + " bar");
    }

    return text;
}

std::string spoken_text::fuelReadout(const Readings &r, const Units &units) {
    std::string text;

    if (imperial(units)) {
        const double gallons = units::convert(r.fuelConsumed, units::gal);
        const double miles = units::convert(r.distance, units::mile);

        text = "Using " + number(units::convert(r.fuelRate, units::gal) * 3600.0, 1) + " gallons per hour";
        join(text, count(number(gallons, 2), "gallon", "gallons") + " used");
        join(text, count(number(miles, 1), "mile", "miles") + " driven");
        if (gallons > 0 && miles >= 0.1) {
            join(text, "average " + number(miles / gallons, 1) + " miles per gallon");
        }
    }
    else {
        const double liters = units::convert(r.fuelConsumed, units::L);
        const double kilometers = units::convert(r.distance, units::km);

        text = "Using " + number(units::convert(r.fuelRate, units::L) * 3600.0, 1) + " liters per hour";
        join(text, count(number(liters, 2), "liter", "liters") + " used");
        join(text, count(number(kilometers, 1), "kilometer", "kilometers") + " driven");
        if (liters > 0 && kilometers >= 0.1) {
            join(text, "average " + number(100.0 * liters / kilometers, 1) + " liters per 100 kilometers");
        }
    }

    return text;
}

std::string spoken_text::infoReadout(const Readings &r, const Units &units) {
    if (!r.hasEngine) return "No engine loaded";

    std::string text = r.engineName;
    join(text, std::to_string(r.cylinders) + (r.cylinders == 1 ? " cylinder" : " cylinders"));

    if (r.displacement < units::volume(1.0, units::L)) {
        join(text, number(units::convert(r.displacement, units::cc)) + " cc");
    }
    else {
        join(text, number(units::convert(r.displacement, units::L), 1) + " liters");
    }

    if (imperial(units)) {
        join(text, number(units::convert(r.displacement, units::cubic_inches)) + " cubic inches");
    }

    join(text, "redline " + engineSpeed(r.redline));

    return text;
}

std::string spoken_text::audioReadout(const Readings &r, const Units &) {
    std::string text = "Volume " + number(r.volume, 2);
    join(text, "convolution " + number(r.convolution, 2));
    if (!r.leveler.empty()) join(text, "leveler " + r.leveler);
    join(text, "simulation frequency " + number(r.simulationFrequency) + " hertz");
    if (r.frameRate > 0) {
        join(text, load(r.simulationLoad));
        join(text, count(number(r.frameRate), "frame", "frames") + " per second");
    }

    return text;
}

std::string spoken_text::simulationLoad(double simulationLoad) {
    return "Simulation " + load(simulationLoad);
}
