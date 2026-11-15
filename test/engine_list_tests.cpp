#include <gtest/gtest.h>

#include "../include/engine_list.h"

#include <filesystem>
#include <fstream>

using namespace engine_list;
namespace fs = std::filesystem;

namespace {
    // A scratch folder with a few scripts, removed afterwards
    class ScriptFolder {
    public:
        ScriptFolder() {
            m_path = fs::temp_directory_path() / ("engine_list_test_" + std::to_string(std::rand()));
            fs::create_directories(m_path / "sub");
        }

        ~ScriptFolder() {
            std::error_code ec;
            fs::remove_all(m_path, ec);
        }

        void write(const std::string &file, const std::string &contents) {
            std::ofstream(m_path / file) << contents;
        }

        std::string path() const { return m_path.string(); }

    private:
        fs::path m_path;
    };

    std::string engineScript(const std::string &name) {
        return
            "import \"engine_sim.mr\"\n"
            "public node my_engine {\n"
            "    fuel fuel(name: \"Diesel\")\n"
            "    engine engine(\n"
            "        name: \"" + name + "\",\n"
            "        starter_torque: 50 * units.lb_ft\n"
            "    )\n"
            "}\n"
            "public node main { set_engine(my_engine()) }\n";
    }
}

TEST(EngineListTests, NameComesFromTheEngineDeclaration) {
    // The fuel's name comes first in the file but isn't the engine's
    EXPECT_EQ(engineName(engineScript("Honda B18C5 [VTEC, I4]")), "Honda B18C5 [VTEC, I4]");

    // filename: is not name:
    EXPECT_EQ(engineName(
        "impulse_response ir(filename: \"a.wav\")\nengine engine(\n    name: \"GM LS\")\n"), "GM LS");

    // A part library declares no engine
    EXPECT_EQ(engineName("public node radial_head {\n    cylinder_head head()\n}\n"), "");
}

TEST(EngineListTests, SpokenNamesHaveNoDecoration) {
    EXPECT_EQ(spokenName("Honda B18C5 [VTEC, I4]"), "Honda B18C5, VTEC, I4");
    EXPECT_EQ(spokenName("Merlin V-1650-9 [V12] (NA)"), "Merlin V-1650-9, V12 (NA)");
    EXPECT_EQ(spokenName("[I8]"), "I8");
    EXPECT_EQ(spokenName("~VQ35DE~"), "VQ35DE");
    EXPECT_EQ(spokenName("4AGE 16v ITB       11K/rpm --"), "4AGE 16v ITB 11K/rpm");
    EXPECT_EQ(spokenFileName("C:/engines/01_subaru_ej25_eh.mr"), "01 subaru ej25 eh");
}

TEST(EngineListTests, FindsEnginesSortedAndTellsDuplicatesApart) {
    ScriptFolder folder;
    folder.write("zz_ls.mr", engineScript("GM LS"));
    folder.write("01_subaru_ej25_eh.mr", engineScript("Subaru EJ25"));
    folder.write("sub/02_subaru_ej25_uh.mr", engineScript("Subaru EJ25"));
    folder.write("turbo.mr", engineScript("Subaru EJ25 Turbo"));
    folder.write("radial.mr", "public node radial_head {\n}\n");
    folder.write("notes.txt", engineScript("Not a script"));

    const std::vector<Entry> entries = find({ { folder.path(), true, "" } });
    ASSERT_EQ(entries.size(), 4u);
    EXPECT_EQ(entries[0].name, "GM LS");
    EXPECT_EQ(entries[1].name, "Subaru EJ25, file 01 subaru ej25 eh");
    EXPECT_EQ(entries[2].name, "Subaru EJ25, file 02 subaru ej25 uh");
    EXPECT_EQ(entries[3].name, "Subaru EJ25 Turbo");

    // Not recursive, with a place added to each name
    const std::vector<Entry> top = find({ { folder.path(), false, ", in Downloads" } });
    ASSERT_EQ(top.size(), 3u);
    EXPECT_EQ(top[0].name, "GM LS, in Downloads");

    // A missing folder is skipped
    EXPECT_TRUE(find({ { folder.path() + "/missing", true, "" } }).empty());
}

TEST(EngineListTests, FindsTheLoadedEngineAndStepsAround) {
    ScriptFolder folder;
    folder.write("01_subaru_ej25_eh.mr", engineScript("Subaru EJ25"));
    folder.write("02_subaru_ej25_uh.mr", engineScript("Subaru EJ25"));
    folder.write("ls.mr", engineScript("GM LS"));

    const std::vector<Entry> entries = find({ { folder.path(), true, "" } });
    ASSERT_EQ(entries.size(), 3u);
    const std::string second = (fs::path(folder.path()) / "02_subaru_ej25_uh.mr").string();

    // The same file wins over the first engine with the same name
    EXPECT_EQ(indexOf(entries, second, "Subaru EJ25"), 2);
    EXPECT_EQ(indexOf(entries, "", "Subaru EJ25"), 1);
    EXPECT_EQ(indexOf(entries, "", "GM LS"), 0);
    EXPECT_EQ(indexOf(entries, "", "Something else"), -1);

    // Wraps around both ends; from nothing, forward is the first entry
    EXPECT_EQ(step(2, 1, 3), 0);
    EXPECT_EQ(step(0, -1, 3), 2);
    EXPECT_EQ(step(1, 1, 3), 2);
    EXPECT_EQ(step(-1, 1, 3), 0);
    EXPECT_EQ(step(-1, -1, 3), 2);
    EXPECT_EQ(step(0, 1, 0), -1);

    // The list opens on the same file too
    Browser list;
    list.open(entries, "Subaru EJ25", second);
    EXPECT_EQ(list.index(), 2);
}

TEST(EngineListTests, FindsEveryBundledEngine) {
    const std::string engines = (fs::path(ENGINE_SIM_SOURCE_DIR) / "assets" / "engines").string();
    const std::vector<Entry> entries = find({ { engines, true, "" } });

    // Every script under assets/engines except the two radial part libraries
    int scripts = 0;
    for (const auto &file : fs::recursive_directory_iterator(engines)) {
        if (file.path().extension() == ".mr") ++scripts;
    }
    EXPECT_EQ(static_cast<int>(entries.size()), scripts - 2);

    bool honda = false;
    for (const Entry &entry : entries) {
        EXPECT_FALSE(entry.name.empty()) << entry.path;
        EXPECT_EQ(entry.name.find('['), std::string::npos) << entry.name;
        if (entry.name == "Honda B18C5, VTEC, I4") honda = true;
    }
    EXPECT_TRUE(honda);
}

TEST(EngineListTests, BrowsingAndTypingToJump) {
    Browser list;
    list.open({
        { "GM LS", "a" }, { "Honda B18C5", "b" }, { "Honda GX25", "c" },
        { "Hayabusa", "d" }, { "Subaru EJ25", "e" }, { "Subaru EJ25 Turbo", "f" },
        { "Open another file", "" } },
        "Subaru EJ25");
    ASSERT_TRUE(list.isOpen());
    EXPECT_EQ(list.index(), 4);
    EXPECT_EQ(list.describe(), "Subaru EJ25, 5 of 7");

    // Stops at the ends
    list.move(10);
    EXPECT_EQ(list.index(), 6);
    list.move(-10);
    EXPECT_EQ(list.index(), 0);

    // The same letter steps through names starting with it
    EXPECT_TRUE(list.type('h', 0.0));
    EXPECT_EQ(list.current().path, "b");
    EXPECT_TRUE(list.type('h', 0.3));
    EXPECT_EQ(list.current().path, "c");
    EXPECT_TRUE(list.type('h', 0.6));
    EXPECT_EQ(list.current().path, "d");
    EXPECT_TRUE(list.type('h', 0.9));
    EXPECT_EQ(list.current().path, "b");

    // Letters typed quickly narrow the search
    EXPECT_TRUE(list.type('h', 3.0));
    EXPECT_TRUE(list.type('A', 3.2));
    EXPECT_EQ(list.current().path, "d");

    // No match: stays put
    EXPECT_FALSE(list.type('x', 5.0));
    EXPECT_EQ(list.current().path, "d");

    list.close();
    EXPECT_FALSE(list.isOpen());
}
