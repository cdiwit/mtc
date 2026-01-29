#include "PathUtils.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

namespace PathUtils {

fs::path GetExecutableDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path();
    
#elif defined(__APPLE__)
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return fs::canonical(fs::path(path)).parent_path();
    }
    return fs::current_path();
    
#elif defined(__linux__)
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return fs::path(path).parent_path();
    }
    return fs::current_path();
    
#else
    return fs::current_path();
#endif
}

fs::path GetDataDir() {
    return GetExecutableDir() / "data";
}

bool EnsureDirectory(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return fs::create_directories(path);
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

bool Exists(const fs::path& path) {
    try {
        return fs::exists(path);
    }
    catch (...) {
        return false;
    }
}

fs::path Normalize(const fs::path& path) {
    try {
        if (fs::exists(path)) {
            return fs::canonical(path);
        }
        return fs::absolute(path);
    }
    catch (...) {
        return path;
    }
}

} // namespace PathUtils
