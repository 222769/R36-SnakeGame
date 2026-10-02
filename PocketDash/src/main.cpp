// Pocket Dash — a colourful top-down arcade adventure for the R36S handheld.

#include "Game.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

void printUsage() {
    std::printf(
        "Pocket Dash %s\n"
        "Usage: pocketdash [options]\n"
        "  --fullscreen        start fullscreen\n"
        "  --windowed          start in a window\n"
        "  --scale N           window scale (default 2 on desktop)\n"
        "  --software          force the software renderer\n"
        "  --debug             start with the debug overlay (F1 toggles)\n"
        "  --play              skip the title screen\n"
        "  --difficulty D      relaxed, normal or challenge (saved)\n"
        "  --frames N          quit after N frames\n"
        "  --screenshot FILE   save the last frame as PNG (use with --frames)\n"
        "  --smoke-test        run the scripted self-test and exit (0 = pass)\n"
        "  --help              show this help\n",
        POCKETDASH_VERSION);
}

} // namespace

int main(int argc, char* argv[]) {
    pd::GameOptions options;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(arg, "--fullscreen")) options.fullscreen = 1;
        else if (!std::strcmp(arg, "--windowed")) options.fullscreen = 0;
        else if (!std::strcmp(arg, "--scale")) options.windowScale = std::atoi(next());
        else if (!std::strcmp(arg, "--software")) options.softwareRenderer = true;
        else if (!std::strcmp(arg, "--debug")) options.debug = true;
        else if (!std::strcmp(arg, "--play")) options.skipTitle = true;
        else if (!std::strcmp(arg, "--difficulty")) options.difficulty = next();
        else if (!std::strcmp(arg, "--frames")) options.maxFrames = std::atoi(next());
        else if (!std::strcmp(arg, "--screenshot")) options.screenshotPath = next();
        else if (!std::strcmp(arg, "--smoke-test")) options.smokeTest = true;
        else if (!std::strcmp(arg, "--help") || !std::strcmp(arg, "-h")) {
            printUsage();
            return 0;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", arg);
            printUsage();
            return 2;
        }
    }

    try {
        pd::Game game(options);
        return game.run();
    } catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Fatal error: %s", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pocket Dash", e.what(), nullptr);
        return 1;
    }
}
