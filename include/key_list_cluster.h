#ifndef ATG_ENGINE_SIM_KEY_LIST_CLUSTER_H
#define ATG_ENGINE_SIM_KEY_LIST_CLUSTER_H

#include "ui_element.h"

// The key list (F1 or the KEYS button), drawn over the engine view, the
// oscilloscopes and the gauges on the main screen
class KeyListCluster : public UiElement {
public:
    KeyListCluster();
    virtual ~KeyListCluster();

    virtual void render();
};

#endif /* ATG_ENGINE_SIM_KEY_LIST_CLUSTER_H */
