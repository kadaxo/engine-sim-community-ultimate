#ifndef ATG_ENGINE_SIM_KEY_LIST_H
#define ATG_ENGINE_SIM_KEY_LIST_H

#include <string>
#include <vector>

// The keys the app's key list shows (F1 or the KEYS button), in groups.
// Keep it in step with the controls table in README.md.
namespace key_list {
    struct Key {
        std::string keys;
        std::string action;
    };

    struct Group {
        std::string title;
        std::vector<Key> keys;
    };

    const std::vector<Group> &groups();
}

#endif /* ATG_ENGINE_SIM_KEY_LIST_H */
