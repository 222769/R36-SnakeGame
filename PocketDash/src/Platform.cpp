#include "Platform.h"

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace pd::platform {

namespace {

std::string withSeparator(std::string path) {
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
    return path;
}

bool looksLikeDataRoot(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "assets", ec) || fs::is_directory(dir / "config", ec);
}

std::string resolveDataRoot() {
    std::vector<fs::path> candidates;
    if (const char* env = std::getenv("POCKETDASH_DATA"); env && *env) candidates.emplace_back(env);

    if (char* base = SDL_GetBasePath()) {
        fs::path exeDir(base);
        SDL_free(base);
        candidates.push_back(exeDir);
        candidates.push_back(exeDir / "..");
        candidates.push_back(exeDir / ".." / "..");
    }

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) candidates.push_back(cwd);

    for (const auto& dir : candidates) {
        if (looksLikeDataRoot(dir)) {
            const fs::path canonical = fs::weakly_canonical(dir, ec);
            return withSeparator((ec ? dir : canonical).string());
        }
    }
    SDL_Log("[platform] Warning: could not find assets/ or config/; using working directory");
    return withSeparator(ec ? std::string(".") : cwd.string());
}

std::string resolveSaveDir() {
    std::error_code ec;
    const fs::path preferred = fs::path(dataRoot()) / "save";
    fs::create_directories(preferred, ec);
    if (!ec && fs::is_directory(preferred, ec)) {
        // Make sure the directory is actually writable (read-only SD card, etc).
        const fs::path probe = preferred / ".write_test";
        if (FILE* f = std::fopen(probe.string().c_str(), "w")) {
            std::fclose(f);
            fs::remove(probe, ec);
            return withSeparator(preferred.string());
        }
    }
    if (char* pref = SDL_GetPrefPath("PocketDash", "PocketDash")) {
        std::string result = withSeparator(pref);
        SDL_free(pref);
        SDL_Log("[platform] Save directory not writable, using %s", result.c_str());
        return result;
    }
    return withSeparator(preferred.string());
}

} // namespace

const char* name() {
#if defined(POCKETDASH_R36S)
    return "R36S (ArkOS)";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

bool isHandheld() {
#if defined(POCKETDASH_R36S)
    return true;
#else
    return false;
#endif
}

bool defaultFullscreen() {
    if (isHandheld()) return true;
#if defined(__linux__)
    // No X11/Wayland session means we are running on a console framebuffer
    // (KMSDRM), which only supports fullscreen anyway.
    const char* x11 = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if ((!x11 || !*x11) && (!wayland || !*wayland)) return true;
#endif
    return false;
}

int defaultWindowScale() { return isHandheld() ? 1 : 2; }

const std::string& dataRoot() {
    static const std::string root = resolveDataRoot();
    return root;
}

const std::string& saveDir() {
    static const std::string dir = resolveSaveDir();
    return dir;
}

std::string dataPath(const std::string& relative) { return dataRoot() + relative; }

} // namespace pd::platform
