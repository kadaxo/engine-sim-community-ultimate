#ifndef ATG_ENGINE_SIM_SPOKEN_TEXT_H
#define ATG_ENGINE_SIM_SPOKEN_TEXT_H

#include <string>
#include <vector>

// Text for screen reader announcements. Values come in the simulator's SI
// units and are spoken in the units the gauges use (ApplicationSettings).
namespace spoken_text {

    // At most `decimals` decimals, without trailing zeros: 0.42, 1250, -3.5
    std::string number(double value, int decimals = 0);

    struct Units {
        std::string power = "hp";
        std::string torque = "lb-ft";
        std::string speed = "mph";
        std::string pressure = "inHg";
    };

    enum class Category {
        Event,      // something the user did or that happened: always spoken
        Adjustment, // a value being dialled in: spoken once it settles
        Throttle,   // throttle changes: only when every change is wanted
        View,       // display-only changes: only when every change is wanted
        Silent      // announced another way
    };

    struct Message {
        std::string text;
        Category category = Category::Event;
    };

    // The info panel's log message, reworded for speech
    Message fromLogMessage(const std::string &logMessage);

    enum class Verbosity {
        Normal,       // events and settled adjustments
        All,          // also throttle and view changes
        ReadoutsOnly  // nothing unless asked for with a readout key
    };

    // What the J key changes to. From silence it turns speech on at Normal;
    // while speaking it steps Normal, All, ReadoutsOnly and then off.
    struct SpeechSetting {
        bool on = false;
        Verbosity verbosity = Verbosity::Normal;
        std::string announcement; // said before the change takes effect
    };
    SpeechSetting speechKey(bool speaking, Verbosity current);

    // Turns the stream of log messages into announcements: drops the repeats
    // of a message that's logged every frame while a key is held, holds an
    // adjustment back until its value settles, and leaves out what the
    // verbosity excludes. Times are in seconds.
    class Announcer {
    public:
        static constexpr double RepeatWindow = 0.25;
        static constexpr double SettleTime = 0.3;

        void setVerbosity(Verbosity verbosity) { m_verbosity = verbosity; }
        Verbosity getVerbosity() const { return m_verbosity; }
        bool announcesEvents() const { return m_verbosity != Verbosity::ReadoutsOnly; }

        // Text to announce now, or an empty string
        std::string onLogMessage(const std::string &logMessage, double time);

        // A settled adjustment to announce now, or an empty string
        std::string poll(double time);

    private:
        Verbosity m_verbosity = Verbosity::Normal;

        std::string m_lastMessage;
        double m_lastMessageTime = -1.0;

        std::string m_pending;
        double m_pendingTime = 0.0;
    };

    // The first error in a script compiler log, as a sentence. Empty if the
    // log has no recognizable error.
    std::string scriptError(const std::string &errorLog);

    // Everything the readout keys report. SI units throughout (radians per
    // second, meters per second, newton meters, watts, pascals, cubic meters).
    struct Readings {
        // Engine
        bool hasEngine = false;
        std::string engineName;
        int cylinders = 0;
        double displacement = 0.0;
        double redline = 0.0;
        double engineSpeed = 0.0;
        double throttle = 0.0;
        bool ignition = false;
        bool starter = false;

        // Vehicle
        double vehicleSpeed = 0.0;
        int gear = -1;
        double clutch = 1.0;
        double brake = 0.0;

        // Dyno
        bool dynoEnabled = false;
        bool dynoHold = false;
        double dynoSpeed = 0.0;
        double torque = 0.0;
        double power = 0.0;

        // Air
        double manifoldPressure = 0.0;
        double airFlow = 0.0;
        double volumetricEfficiency = 0.0;
        double airFuelRatio = 0.0;
        bool hasCompressor = false;
        double boost = 0.0;

        // Fuel
        double fuelConsumed = 0.0;
        double distance = 0.0;
        double fuelRate = 0.0;

        // Audio and simulation
        double volume = 0.0;
        double convolution = 0.0;
        std::string leveler;
        double simulationFrequency = 0.0;
        double frameRate = 0.0;         // 0 when unknown: performance left out
        double simulationLoad = 0.0;    // fraction of real time; above 1 can't keep up
    };

    std::string engineReadout(const Readings &r, const Units &units);
    std::string vehicleReadout(const Readings &r, const Units &units);
    std::string dynoReadout(const Readings &r, const Units &units);
    std::string airReadout(const Readings &r, const Units &units);
    std::string fuelReadout(const Readings &r, const Units &units);
    std::string infoReadout(const Readings &r, const Units &units);
    std::string audioReadout(const Readings &r, const Units &units);

    // "Simulation load 45 percent", with a warning near or over the limit
    std::string simulationLoad(double load);

    struct DynoPoint {
        double rpm;
        double torque;
        double power;
    };

    // "Dyno run finished. Peak power ... at ... rpm, peak torque ... at ...
    // rpm", or an empty string for a run with fewer than two points
    std::string dynoSummary(const std::vector<DynoPoint> &run, const Units &units);

    std::string torque(double torque, const Units &units);
    std::string power(double power, const Units &units);
    std::string engineSpeed(double angularSpeed);

} /* namespace spoken_text */

#endif /* ATG_ENGINE_SIM_SPOKEN_TEXT_H */
