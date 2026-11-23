#include "../include/open_in_browser.h"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

void openInBrowser(const std::string &path) {
#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (length <= 0) return;

    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), length);
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open \"" + path + "\" &").c_str());
#else
    std::system(("xdg-open \"" + path + "\" >/dev/null 2>&1 &").c_str());
#endif
}
