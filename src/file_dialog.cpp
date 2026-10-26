#include "../include/file_dialog.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#endif

#include <algorithm>

#ifdef _WIN32

bool openScriptFileDialog(
    const std::string &title,
    const std::string &initialDirectory,
    void *ownerWindow,
    std::string *path)
{
    char fileName[MAX_PATH] = { 0 };

    std::string directory = initialDirectory;
    std::replace(directory.begin(), directory.end(), '/', '\\');

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    ofn.lpstrFilter = "Engine Simulator scripts (*.mr)\0*.mr\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = directory.empty() ? nullptr : directory.c_str();
    ofn.lpstrTitle = title.c_str();
    // The app resolves its data relative to the working directory, which
    // the dialog would otherwise change
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return false;

    *path = fileName;
    std::replace(path->begin(), path->end(), '\\', '/');
    return true;
}

#else

namespace {
    // Wraps s in single quotes for the shell
    std::string shellQuote(const std::string &s) {
        std::string quoted = "'";
        for (const char c : s) {
            if (c == '\'') quoted += "'\\''";
            else quoted += c;
        }

        return quoted + "'";
    }
}

bool openScriptFileDialog(
    const std::string &title,
    const std::string &initialDirectory,
    void *ownerWindow,
    std::string *path)
{
    (void)ownerWindow;

#ifdef __APPLE__
    const std::string command =
        "osascript -e " + shellQuote(
            "POSIX path of (choose file with prompt \"" + title + "\""
            + (initialDirectory.empty()
                ? std::string()
                : " default location POSIX file \"" + initialDirectory + "\"")
            + ")")
        + " 2>/dev/null";
#else
    // zenity, as used for engine import in Fraxoor/engine-sim-linux
    const std::string command =
        "zenity --file-selection --title=" + shellQuote(title)
        + " --file-filter=" + shellQuote("Engine Simulator scripts | *.mr")
        + (initialDirectory.empty()
            ? std::string()
            : " --filename=" + shellQuote(initialDirectory + "/"))
        + " 2>/dev/null";
#endif

    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return false;

    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    if (result.empty()) return false;

    *path = result;
    return true;
}

#endif
