#include "../include/engine_loader.h"

#include "../include/data_root.h"
#include "../include/engine_script.h"
#include "../include/transmission.h"
#include "../include/units.h"
#include "../include/vehicle.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace {

    // The piranha-based compiler stores its output in a process-wide
    // singleton, so compiles must not run concurrently
    std::mutex &compilerMutex() {
        static std::mutex m;
        return m;
    }

    bool compileAndRun(
        const std::string &path,
        const std::vector<piranha::IrPath> &searchPaths,
        es_script::Compiler::Output *output,
        std::ostream &log)
    {
        // initialize() also clears the previous compile's output, so nothing
        // leaks into this one if the script omits a node
        es_script::Compiler compiler;
        compiler.initialize(searchPaths);
        const bool compiled = compiler.compile(path.c_str(), log);
        if (compiled) {
            *output = compiler.execute();
        }

        compiler.destroy();
        return compiled;
    }

    std::string readTextFile(const std::string &path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

} /* namespace */

Vehicle *engine_loader::createDefaultVehicle() {
    Vehicle::Parameters vehParams;
    vehParams.mass = units::mass(1597, units::kg);
    vehParams.diffRatio = 3.42;
    vehParams.tireRadius = units::distance(10, units::inch);
    vehParams.dragCoefficient = 0.25;
    vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
    vehParams.rollingResistance = 2000.0;

    Vehicle *vehicle = new Vehicle;
    vehicle->initialize(vehParams);
    return vehicle;
}

Transmission *engine_loader::createDefaultTransmission() {
    const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
    Transmission::Parameters tParams;
    tParams.GearCount = 6;
    tParams.GearRatios = gearRatios;    // copied by initialize()
    tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);

    Transmission *transmission = new Transmission;
    transmission->initialize(tParams);
    return transmission;
}

void engine_loader::addDefaults(es_script::Compiler::Output *output) {
    if (output->vehicle == nullptr) output->vehicle = createDefaultVehicle();
    if (output->transmission == nullptr) output->transmission = createDefaultTransmission();
}

bool engine_loader::loadScript(
    const std::string &path,
    const Options &options,
    es_script::Compiler::Output *output,
    std::ostream &log,
    std::string *error)
{
    std::vector<piranha::IrPath> searchPaths;
    for (const std::string &searchPath : options.searchPaths) {
        searchPaths.push_back(searchPath.c_str());
    }

    std::lock_guard<std::mutex> lock(compilerMutex());

    if (!compileAndRun(path, searchPaths, output, log)) {
        *error = "failed to compile '" + path + "'";
        return false;
    }

    if (output->engine != nullptr || !options.runEngineFiles) return true;

    // Engine files from assets/engines only define nodes. Run them the way
    // LOAD ENGINE does, from a script that imports the file and calls it.
    const std::string call = engineScriptCall(readTextFile(path));
    if (call.empty()) return true;

    std::error_code ec;
    const fs::path engineFile = fs::weakly_canonical(fs::absolute(path, ec), ec);
    const fs::path wrapper = fs::temp_directory_path(ec)
        / ("engine-sim-script-" + std::to_string(std::random_device()()) + ".mr");
    {
        std::ofstream script(wrapper, std::ios::out);
        script << "import \"engine_sim.mr\"\n";
        script << "import \"" << engineFile.generic_string() << "\"\n\n";
        script << call << "\n";
        if (!script) {
            *error = "can't write '" + wrapper.string() + "'";
            return false;
        }
    }

    const bool ran = compileAndRun(wrapper.generic_string(), searchPaths, output, log);
    fs::remove(wrapper, ec);
    if (!ran) {
        *error = "'" + path + "' doesn't call set_engine(), and running it as an engine file ("
            + call + ") failed";
        return false;
    }

    return true;
}

std::string engine_loader::findScriptLibrary(const std::string &exeDir) {
    const fs::path root = data_root::find(std::getenv("ENGINE_SIM_DATA_ROOT"), "", fs::path(exeDir));
    std::error_code ec;
    if (root.empty() || !fs::is_regular_file(root / "es" / "engine_sim.mr", ec)) {
        return std::string();
    }

    return (root / "es").generic_string() + "/";
}

engine_loader::Options engine_loader::toolOptions(const std::string &scriptPath, const char *argv0) {
    Options options;

    fs::path scriptDir = fs::path(scriptPath).parent_path();
    if (scriptDir.empty()) scriptDir = ".";
    options.searchPaths.push_back(scriptDir.generic_string() + "/");

    const std::string library = findScriptLibrary(data_root::executableDirectory(argv0).string());
    if (!library.empty()) options.searchPaths.push_back(library);

    return options;
}
