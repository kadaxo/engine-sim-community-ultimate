#include "../include/engine_list.h"

#include "../include/engine_script.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace {
    namespace fs = std::filesystem;

    std::string readFile(const fs::path &path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    std::string lower(const std::string &text) {
        std::string result = text;
        for (char &c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

    // What sorting and type-to-jump compare: lower case, without leading
    // punctuation ("~VQ35DE~" is found by typing v)
    std::string key(const std::string &name) {
        size_t start = 0;
        while (start < name.size() && !std::isalnum(static_cast<unsigned char>(name[start]))) ++start;
        return lower(name.substr(start));
    }

    bool declaresEngine(const std::string &script, std::string *name) {
        static const std::regex declaration(R"((^|[^\w])engine\s+[A-Za-z_]\w*\s*\()");
        static const std::regex nameInput(R"re((^|[^\w])name\s*:\s*"([^"]*)")re");

        std::smatch match;
        if (!std::regex_search(script, match, declaration)) return false;

        const std::string rest = match.suffix().str();
        std::smatch nameMatch;
        *name = std::regex_search(rest, nameMatch, nameInput) ? nameMatch[2].str() : std::string();
        return true;
    }
}

std::string engine_list::engineName(const std::string &script) {
    std::string name;
    return declaresEngine(script, &name) ? name : std::string();
}

std::string engine_list::spokenName(const std::string &name) {
    std::string text;
    for (const char c : name) {
        if (c == '[') text += ", ";
        else if (c == ']' || c == '~') continue;
        else if (std::isspace(static_cast<unsigned char>(c))) text += ' ';
        else text += c;
    }

    // Collapse the spaces and commas the replacements leave behind
    std::string result;
    for (const char c : text) {
        if (c == ' ' && (result.empty() || result.back() == ' ')) continue;
        if (c == ',' && !result.empty() && result.back() == ' ') result.pop_back();
        if (c == ',' && !result.empty() && result.back() == ',') continue;
        result += c;
    }

    auto trimmed = [](char c) { return c == ' ' || c == ',' || c == '-'; };
    while (!result.empty() && trimmed(result.back())) result.pop_back();
    size_t start = 0;
    while (start < result.size() && trimmed(result[start])) ++start;

    return result.substr(start);
}

std::string engine_list::spokenFileName(const std::string &path) {
    std::string stem = fs::path(path).stem().string();
    for (char &c : stem) {
        if (c == '_' || c == '-' || c == '.') c = ' ';
    }

    return spokenName(stem);
}

std::vector<engine_list::Entry> engine_list::find(const std::vector<Folder> &folders) {
    struct Found {
        std::string name;
        std::string path;
        std::string where;
    };

    std::vector<Found> found;
    std::vector<fs::path> seen;
    for (const Folder &folder : folders) {
        std::error_code ec;
        if (!fs::is_directory(folder.path, ec)) continue;

        auto addIfEngine = [&](const fs::directory_entry &entry) {
            std::error_code ec;
            if (!entry.is_regular_file(ec) || lower(entry.path().extension().string()) != ".mr") return;

            // The same folder can be reached twice (user data inside the data root)
            const fs::path canonical = fs::weakly_canonical(entry.path(), ec);
            if (std::find(seen.begin(), seen.end(), canonical) != seen.end()) return;
            seen.push_back(canonical);

            const std::string script = readFile(entry.path());
            std::string name;
            if (!declaresEngine(script, &name) || engineScriptCall(script).empty()) return;

            name = spokenName(name);
            if (name.empty()) name = spokenFileName(entry.path().string());
            found.push_back({ name, entry.path().string(), folder.where });
        };

        auto consider = [&](const fs::directory_entry &entry) {
            // A file name the narrow path API can't represent is skipped
            try { addIfEngine(entry); }
            catch (const std::exception &) { /* skipped */ }
        };

        if (folder.recursive) {
            for (auto it = fs::recursive_directory_iterator(folder.path, ec);
                !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            {
                consider(*it);
            }
        }
        else {
            for (auto it = fs::directory_iterator(folder.path, ec);
                !ec && it != fs::directory_iterator(); it.increment(ec))
            {
                consider(*it);
            }
        }
    }

    std::map<std::string, int> uses;
    for (const Found &f : found) ++uses[key(f.name)];

    // By name, then engines sharing a name by file, so they stay together
    std::vector<std::pair<std::string, Entry>> named;
    for (const Found &f : found) {
        std::string name = f.name;
        if (uses[key(f.name)] > 1) name += ", file " + spokenFileName(f.path);
        named.push_back({ key(f.name), { name + f.where, f.path } });
    }

    std::stable_sort(named.begin(), named.end(), [](const auto &a, const auto &b) {
        return (a.first != b.first) ? a.first < b.first : key(a.second.name) < key(b.second.name);
    });

    std::vector<Entry> entries;
    for (auto &n : named) entries.push_back(std::move(n.second));
    return entries;
}

std::vector<engine_list::Folder> engine_list::standardFolders(
    const std::string &dataRoot,
    const std::string &userData)
{
    std::vector<Folder> folders;
    auto add = [&](const fs::path &path, bool recursive, const std::string &where) {
        // A folder whose name the narrow path API can't represent is skipped
        try {
            std::error_code ec;
            folders.push_back({ fs::absolute(path, ec).string(), recursive, where });
        }
        catch (const std::exception &) { /* skipped */ }
    };

    add(fs::path(dataRoot) / "assets" / "engines", true, "");
    add(fs::path(userData) / "assets" / "engines", true, "");

    fs::path downloads;
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        downloads = path;
    }
    CoTaskMemFree(path);
#else
    const char *home = std::getenv("HOME");
    if (home != nullptr) downloads = fs::path(home) / "Downloads";
#endif
    if (!downloads.empty()) add(downloads, false, ", in Downloads");

    return folders;
}

int engine_list::indexOf(const std::vector<Entry> &entries, const std::string &path, const std::string &name) {
    const int count = static_cast<int>(entries.size());
    if (!path.empty()) {
        for (int i = 0; i < count; ++i) {
            std::error_code ec;
            if (!entries[i].path.empty() && fs::equivalent(entries[i].path, path, ec)) return i;
        }
    }

    // Loaded engines report their raw name; entries may have ", file ..." or
    // ", in Downloads" added
    const std::string start = key(spokenName(name));
    for (int i = 0; i < count && !start.empty(); ++i) {
        const std::string entry = key(entries[i].name);
        if (entry == start || entry.rfind(start + ",", 0) == 0) return i;
    }

    return -1;
}

int engine_list::step(int current, int direction, int count) {
    if (count <= 0) return -1;
    if (current < 0) return (direction > 0) ? 0 : count - 1;
    return ((current + direction) % count + count) % count;
}

void engine_list::Browser::open(
    std::vector<Entry> entries,
    const std::string &startName,
    const std::string &startPath)
{
    m_entries = std::move(entries);
    m_open = !m_entries.empty();
    m_typed.clear();
    m_lastTypeTime = -10.0;
    m_index = std::max(indexOf(m_entries, startPath, startName), 0);
}

void engine_list::Browser::close() {
    m_open = false;
    m_entries.clear();
}

void engine_list::Browser::move(int delta) {
    moveTo(m_index + delta);
}

void engine_list::Browser::moveTo(int index) {
    if (m_entries.empty()) return;
    m_index = std::clamp(index, 0, size() - 1);
}

bool engine_list::Browser::startsWith(int index, const std::string &prefix) const {
    return key(m_entries[index].name).rfind(prefix, 0) == 0;
}

bool engine_list::Browser::type(char c, double time) {
    if (m_entries.empty()) return false;

    if (time - m_lastTypeTime > 1.0) m_typed.clear();
    m_lastTypeTime = time;
    m_typed += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // "hhh" steps through the h's; "ho" narrows down to names starting "ho"
    const bool repeated = std::all_of(m_typed.begin(), m_typed.end(),
        [&](char t) { return t == m_typed[0]; });
    const std::string prefix = repeated ? m_typed.substr(0, 1) : m_typed;
    const int first = repeated ? m_index + 1 : m_index;

    for (int i = 0; i < size(); ++i) {
        const int candidate = (first + i) % size();
        if (startsWith(candidate, prefix)) {
            m_index = candidate;
            return true;
        }
    }

    return false;
}

std::string engine_list::Browser::describe() const {
    if (m_entries.empty()) return "";
    return current().name + ", " + std::to_string(m_index + 1) + " of " + std::to_string(size());
}
