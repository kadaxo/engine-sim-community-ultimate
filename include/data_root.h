#ifndef ATG_ENGINE_SIM_DATA_ROOT_H
#define ATG_ENGINE_SIM_DATA_ROOT_H

#include <filesystem>
#include <string>

namespace data_root {
    // The folder holding the running program, or an empty path if the
    // system can't say. argv0 is used where there is no system call for it.
    std::filesystem::path executableDirectory(const char *argv0 = nullptr);

    // Whether a folder holds the data the app needs: the bundled scripts
    // (assets/main.mr) and the script library (es/engine_sim.mr).
    bool isDataRoot(const std::filesystem::path &dir);

    // Where the app's data is, in this order:
    //  - environment: the ENGINE_SIM_DATA_ROOT environment variable, if set;
    //  - compiled: the path set with CMake, if it's absolute (an install
    //    into fixed folders);
    //  - the Resources folder of a macOS app bundle;
    //  - the nearest folder at or above the executable that holds the data
    //    (a release package, or a build folder inside the source tree);
    //  - compiled again, relative to the working directory, as it always
    //    was. This is also the answer when nothing else is found.
    // So the app finds its data however it's started (from a shortcut, the
    // Start menu, or another folder), not only from the folder it's in.
    std::filesystem::path find(
        const char *environment,
        const std::string &compiled,
        const std::filesystem::path &exeDir);
} /* namespace data_root */

#endif /* ATG_ENGINE_SIM_DATA_ROOT_H */
