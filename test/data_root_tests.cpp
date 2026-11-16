#include <gtest/gtest.h>

#include "../include/data_root.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {
    // A scratch folder, removed afterwards
    class Scratch {
    public:
        Scratch() {
            m_path = fs::temp_directory_path() / ("data_root_test_" + std::to_string(std::rand()));
            fs::create_directories(m_path);
        }

        ~Scratch() {
            std::error_code ec;
            fs::remove_all(m_path, ec);
        }

        // Makes `dir` (inside the scratch folder) look like the app's data
        fs::path makeDataRoot(const fs::path &dir) const {
            const fs::path root = m_path / dir;
            fs::create_directories(root / "assets");
            fs::create_directories(root / "es");
            std::ofstream(root / "assets" / "main.mr") << "\n";
            std::ofstream(root / "es" / "engine_sim.mr") << "\n";
            return root;
        }

        fs::path dir(const fs::path &dir) const {
            fs::create_directories(m_path / dir);
            return m_path / dir;
        }

    private:
        fs::path m_path;
    };

    bool same(const fs::path &a, const fs::path &b) {
        return fs::equivalent(a, b);
    }
} /* namespace */

TEST(DataRootTests, FindsTheRunningTestProgram) {
    const fs::path exeDir = data_root::executableDirectory();
    ASSERT_FALSE(exeDir.empty());
    EXPECT_TRUE(fs::is_directory(exeDir));
}

TEST(DataRootTests, ReleasePackageUsesTheExecutableFolder) {
    // engine-sim/engine-sim-app.exe next to assets and es
    Scratch s;
    const fs::path root = s.makeDataRoot("engine-sim");
    EXPECT_TRUE(same(data_root::find(nullptr, "..", root), root));
}

TEST(DataRootTests, BuildFoldersFindTheSourceTree) {
    // engine-sim/build/engine-sim-app (Ninja, make) and
    // engine-sim/build/Release/engine-sim-app.exe (Visual Studio)
    Scratch s;
    const fs::path root = s.makeDataRoot("engine-sim");
    EXPECT_TRUE(same(data_root::find(nullptr, "..", s.dir("engine-sim/build")), root));
    EXPECT_TRUE(same(data_root::find(nullptr, "..", s.dir("engine-sim/build/Release")), root));
}

TEST(DataRootTests, MacAppBundleUsesItsResources) {
    Scratch s;
    const fs::path resources = s.makeDataRoot("engine-sim.app/Contents/Resources");
    const fs::path exeDir = s.dir("engine-sim.app/Contents/MacOS");
    EXPECT_TRUE(same(data_root::find(nullptr, "..", exeDir), resources));
}

TEST(DataRootTests, EnvironmentVariableComesFirst) {
    Scratch s;
    const fs::path root = s.makeDataRoot("engine-sim");
    EXPECT_EQ(data_root::find("/somewhere/else", "..", root), fs::path("/somewhere/else"));
}

TEST(DataRootTests, EmptyEnvironmentVariableIsIgnored) {
    Scratch s;
    const fs::path root = s.makeDataRoot("engine-sim");
    EXPECT_TRUE(same(data_root::find("", "..", root), root));
}

TEST(DataRootTests, AbsoluteBuildSettingComesBeforeSearching) {
    // An install into fixed folders, such as /usr/share/engine-sim
    Scratch s;
    const fs::path installed = s.makeDataRoot("share/engine-sim");
    const fs::path other = s.makeDataRoot("engine-sim");
    EXPECT_EQ(data_root::find(nullptr, installed.string(), other), installed);
}

TEST(DataRootTests, FallsBackToTheBuildSettingWhenNothingIsFound) {
    // Relative to the working directory, as before
    Scratch s;
    EXPECT_EQ(data_root::find(nullptr, "..", s.dir("empty/bin")), fs::path(".."));
    EXPECT_EQ(data_root::find(nullptr, "..", fs::path()), fs::path(".."));
}

TEST(DataRootTests, NeedsBothTheScriptsAndTheLibrary) {
    Scratch s;
    const fs::path half = s.dir("half/assets");
    std::ofstream(half / "main.mr") << "\n";
    EXPECT_FALSE(data_root::isDataRoot(half.parent_path()));
    EXPECT_TRUE(data_root::isDataRoot(s.makeDataRoot("full")));
}
