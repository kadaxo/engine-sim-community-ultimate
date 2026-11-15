#ifndef ATG_ENGINE_SIM_ENGINE_LIST_H
#define ATG_ENGINE_SIM_ENGINE_LIST_H

#include <string>
#include <vector>

// The spoken engine list (O with speech on, see docs/accessibility.html):
// finds engine scripts, names them from the scripts themselves, and keeps the
// list's position. Nothing here draws or speaks, so it can be tested alone.
namespace engine_list {
    // The name the script gives its engine (the first name: after an engine
    // declaration), or an empty string if the script declares no engine. Part
    // libraries such as radial.mr declare none.
    std::string engineName(const std::string &script);

    // "Honda B18C5 [VTEC, I4]" -> "Honda B18C5, VTEC, I4": no brackets or
    // decoration for a screen reader to spell out
    std::string spokenName(const std::string &name);

    // "01_subaru_ej25_eh.mr" -> "01 subaru ej25 eh"
    std::string spokenFileName(const std::string &path);

    struct Entry {
        std::string name;   // as spoken
        std::string path;   // empty for "another file", which opens the file dialog
    };

    struct Folder {
        std::string path;
        bool recursive;
        std::string where;  // appended to each name, e.g. ", in Downloads"
    };

    // Every engine script in the folders, sorted by name. Engines that share a
    // name get their file name added so they can be told apart. A missing
    // folder is skipped.
    std::vector<Entry> find(const std::vector<Folder> &folders);

    // The folders the engine list and [ / ] use: assets/engines under the
    // data root and the user data folder, and the Downloads folder, where
    // browsers save the engine builder's files
    std::vector<Folder> standardFolders(const std::string &dataRoot, const std::string &userData);

    // The entry for the loaded engine: the same file if path is given and
    // listed, otherwise the first entry with the engine's name. -1 if none.
    int indexOf(const std::vector<Entry> &entries, const std::string &path, const std::string &name);

    // The index `direction` steps away from `current`, wrapping around the
    // ends. From -1 (nothing loaded from the list), forward starts at the
    // first entry and back at the last.
    int step(int current, int direction, int count);

    // Position and type-to-jump in a list of entries
    class Browser {
    public:
        // Starts on the loaded engine (see indexOf), or the first entry
        void open(
            std::vector<Entry> entries,
            const std::string &startName,
            const std::string &startPath = "");
        void close();
        bool isOpen() const { return m_open; }

        int size() const { return static_cast<int>(m_entries.size()); }
        int index() const { return m_index; }
        const Entry &current() const { return m_entries[m_index]; }

        // Moves by delta, stopping at either end
        void move(int delta);
        void moveTo(int index);

        // Letters typed within a second of each other build up a prefix to
        // jump to. The same letter again moves to the next name starting with
        // it. False, and no move, if no name matches.
        bool type(char c, double time);
        const std::string &typed() const { return m_typed; }

        // "Honda B18C5, VTEC, I4, 45 of 103"
        std::string describe() const;

    private:
        bool startsWith(int index, const std::string &prefix) const;

        std::vector<Entry> m_entries;
        bool m_open = false;
        int m_index = 0;
        std::string m_typed;
        double m_lastTypeTime = -10.0;
    };
}

#endif /* ATG_ENGINE_SIM_ENGINE_LIST_H */
