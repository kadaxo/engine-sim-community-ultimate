#ifndef ATG_ENGINE_SIM_RPM_SYNC_H
#define ATG_ENGINE_SIM_RPM_SYNC_H

#include <chrono>

class RpmSync {
public:
    RpmSync();
    ~RpmSync();

    bool initialize(int port = 8765);
    void destroy();
    void update();

    bool hasTarget() const;
    double getTargetRpm() const { return m_targetRpm; }
    int getPort() const { return m_port; }
    bool isInitialized() const { return m_socket >= 0; }

private:
    int m_socket;
    int m_port;
    double m_targetRpm;
    std::chrono::steady_clock::time_point m_lastUpdate;
};

#endif /* ATG_ENGINE_SIM_RPM_SYNC_H */
