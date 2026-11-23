#ifndef ATG_ENGINE_SIM_ENGINE_LOADER_H
#define ATG_ENGINE_SIM_ENGINE_LOADER_H

#include "../scripting/include/compiler.h"

#include <ostream>
#include <string>
#include <vector>

class Transmission;
class Vehicle;

// Loading an engine script, shared by the app, engine-sim-export and the
// probes so they all compile scripts, run engine files and fill in a missing
// car the same way.
namespace engine_loader {

    // The car and gearbox used when a script sets only an engine (the values
    // the app has always used)
    Vehicle *createDefaultVehicle();
    Transmission *createDefaultTransmission();

    // Gives the output a default vehicle and transmission where the script
    // set none. The engine, if any, is left as it is.
    void addDefaults(es_script::Compiler::Output *output);

    struct Options {
        // Folders searched for imports, before the compiler's own fallbacks
        // (es/ next to the working directory)
        std::vector<std::string> searchPaths;

        // A script that sets no engine but defines one (an engine file from
        // assets/engines) is run the way LOAD ENGINE runs it: from a script
        // that imports it and calls its main() or engine node
        bool runEngineFiles = true;
    };

    // Compiles and runs a script. Returns false, with *error set, if it
    // doesn't compile or (with runEngineFiles) the engine file can't be run.
    // A script that compiles but sets no engine returns true with
    // output->engine null. The compiler's messages go to `log`. Safe to call
    // from several threads: the compiler keeps its output in a process-wide
    // object, so compiles take turns.
    bool loadScript(
        const std::string &path,
        const Options &options,
        es_script::Compiler::Output *output,
        std::ostream &log,
        std::string *error);

    // The engine-sim script library (the es folder) for a program in `exeDir`:
    // ENGINE_SIM_DATA_ROOT if it's set, else the first folder at or above
    // `exeDir` holding assets/main.mr and es/engine_sim.mr. Empty if none.
    std::string findScriptLibrary(const std::string &exeDir);

    // Options for a command-line tool running `scriptPath`: imports resolve
    // next to the script first, then in the script library found from the
    // tool's own location (argv[0]), so it runs from any working directory
    Options toolOptions(const std::string &scriptPath, const char *argv0);

} /* namespace engine_loader */

#endif /* ATG_ENGINE_SIM_ENGINE_LOADER_H */
