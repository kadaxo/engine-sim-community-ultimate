#ifndef ATG_ENGINE_SIM_INFO_CLUSTER_H
#define ATG_ENGINE_SIM_INFO_CLUSTER_H

#include "ui_element.h"

#include "engine.h"

#include <string>

class UiButton;

class InfoCluster : public UiElement {
public:
    InfoCluster();
    virtual ~InfoCluster();

    virtual void initialize(EngineSimApplication *app);
    virtual void destroy();

    virtual void update(float dt);
    virtual void render();

    virtual void signal(UiElement *element, Event event);

    void setEngine(Engine *engine) { m_engine = engine; }
    void setLogMessage(const std::string &logMessage) { m_logMessage = logMessage; }
    std::string getLogMessage() const { return m_logMessage; }

protected:
    Bounds getTitleBounds() const;

    Engine *m_engine;

    std::string m_logMessage;

    UiButton *m_loadEngineButton;
    UiButton *m_reloadButton;
    UiButton *m_loadThemeButton;
    UiButton *m_exitButton;
};

#endif /* ATG_ENGINE_SIM_INFO_CLUSTER_H */
