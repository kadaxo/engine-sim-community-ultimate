#include "../include/data_root.h"

#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#include <vector>
#endif

namespace fs = std::filesystem;

fs::path data_root::executableDirectory(const char *argv0) {
    std::error_code ec;

#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, &buffer[0], static_cast<DWORD>(buffer.size()));
        if (n == 0) break;
        if (n < buffer.size()) {
            buffer.resize(n);
            return fs::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        const fs::path p = fs::canonical(fs::path(buffer.data()), ec);
        if (!ec) return p.parent_path();
    }
#else
    const fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.parent_path();
#endif

    if (argv0 != nullptr && argv0[0] != '\0') {
        ec.clear();
        const fs::path p = fs::absolute(argv0, ec);
        if (!ec) return p.parent_path();
    }

    return fs::path();
}

bool data_root::isDataRoot(const fs::path &dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "assets" / "main.mr", ec)
        && fs::is_regular_file(dir / "es" / "engine_sim.mr", ec);
}

fs::path data_root::find(
    const char *environment,
    const std::string &compiled,
    const fs::path &exeDir)
{
    if (environment != nullptr && environment[0] != '\0') {
        return fs::path(environment);
    }

    const fs::path compiledPath(compiled);
    if (compiledPath.is_absolute()) {
        return compiledPath;
    }

    if (!exeDir.empty()) {
        // engine-sim-app.app/Contents/MacOS/engine-sim-app
        const fs::path resources = exeDir.parent_path() / "Resources";
        if (exeDir.filename() == "MacOS" && isDataRoot(resources)) {
            return resources;
        }

        std::error_code ec;
        for (fs::path p = fs::absolute(exeDir, ec); !ec && !p.empty(); p = p.parent_path()) {
            if (isDataRoot(p)) return p;
            if (p == p.parent_path()) break;
        }
    }

    return compiledPath;
}
