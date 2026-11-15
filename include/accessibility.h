#ifndef ATG_ENGINE_SIM_ACCESSIBILITY_H
#define ATG_ENGINE_SIM_ACCESSIBILITY_H

#include "engine_list.h"
#include "screen_reader.h"
#include "spoken_text.h"

#include <fstream>
#include <string>
#include <vector>

class EngineSimApplication;

// Screen reader support. Speaks the info panel's messages and other events,
// and reads the gauges on demand (F5-F9, F11, F12; see docs/accessibility.html).
// It stays silent unless a screen reader is running, J has been pressed, or
// ENGINE_SIM_SPEECH_LOG names a file to write announcements to, so nothing
// changes for anyone else.
class Accessibility {
public:
    Accessibility();
    ~Accessibility();

    void initialize(EngineSimApplication *app);
    void destroy();

    // Once per frame, after the engine controls have been processed
    void update(float dt);

    void onLogMessage(const std::string &message);
    void onScriptLoaded();
    void onScriptFailed(const std::string &errorLogPath);

    // What to say after "Loaded" for the next engine instead of its name, for
    // [ and ]: the engine list's name for it and its place in the list
    void setNextLoadLabel(const std::string &label) { m_nextLoadLabel = label; }

    // The dyno sweep reached redline and switched itself off
    void onDynoRunFinished();

    bool isActive();

    // The spoken engine list (O while speech is on). While it's open it takes
    // the keyboard: processEngineList() runs instead of the usual keys.
    bool isEngineListOpen() const { return m_engineList.isOpen(); }
    void openEngineList();
    void processEngineList();

private:
    // A queued announcement waits for the current one to finish
    void announce(const std::string &text, bool queue = false);
    void event(const std::string &text, bool queue = false);
    void processKeys();
    void detectEngineState(float dt);
    void sampleFuel();
    void recordDynoRun(float dt);
    void watchSimulationFrequency(float dt);
    bool writeDynoReport(std::string *fileName);
    void openGuide();

    spoken_text::Readings read();
    spoken_text::Units units();

    EngineSimApplication *m_app;
    ScreenReader m_screenReader;
    std::ofstream m_log;

    spoken_text::Announcer m_announcer;
    bool m_forced;
    bool m_muted; // turned off with J; nothing is spoken until J is pressed again
    double m_time;
    double m_lastAnnouncementTime;
    std::string m_lastAnnouncement;

    bool m_running;
    double m_runningTimer;
    double m_stoppedTimer;

    double m_fuelSampleTime;
    double m_fuelSample;
    double m_fuelRate;

    std::vector<spoken_text::DynoPoint> m_dynoRun;
    bool m_dynoSweeping;
    double m_dynoTorque;
    double m_dynoPower;

    double m_lastSimulationFrequency;
    double m_loadReportTimer;

    engine_list::Browser m_engineList;
    bool m_engineListHintGiven;
    std::string m_nextLoadLabel;
};

#endif /* ATG_ENGINE_SIM_ACCESSIBILITY_H */
