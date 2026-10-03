cmake_minimum_required(VERSION 3.13)

project(PocketDash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(POCKETDASH_R36S "Build for the R36S / ArkOS handheld (fullscreen default, Cortex-A35 tuning)" OFF)
option(POCKETDASH_BUILD_TESTS "Build the unit tests" ON)

# ---------------------------------------------------------------------------
# SDL2 + satellite libraries
#
# pkg-config is tried first because it works on every Linux distro (including
# older ArkOS/Ubuntu images whose SDL2_image/ttf/mixer ship no CMake configs).
# CMake config packages (vcpkg, MSYS2, SDL dev zips) are used as a fallback.
# ---------------------------------------------------------------------------
add_library(pd_sdl INTERFACE)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PD_SDL QUIET IMPORTED_TARGET sdl2 SDL2_image SDL2_ttf SDL2_mixer)
endif()

if(PD_SDL_FOUND)
    message(STATUS "PocketDash: SDL2 libraries found via pkg-config")
    target_link_libraries(pd_sdl INTERFACE PkgConfig::PD_SDL)
else()
    find_package(SDL2 REQUIRED CONFIG)
    find_package(SDL2_image REQUIRED CONFIG)
    find_package(SDL2_ttf REQUIRED CONFIG)
    find_package(SDL2_mixer REQUIRED CONFIG)
    message(STATUS "PocketDash: SDL2 libraries found via CMake config packages")
    target_link_libraries(pd_sdl INTERFACE
        SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf SDL2_mixer::SDL2_mixer)
endif()

# ---------------------------------------------------------------------------
# Game core (everything except main.cpp) — shared by the game and the tests
# ---------------------------------------------------------------------------
set(POCKETDASH_CORE_SOURCES
    src/AudioManager.cpp
    src/BuiltinLevels.cpp
    src/Camera.cpp
    src/Collectibles.cpp
    src/Draw.cpp
    src/Effects.cpp
    src/Enemy.cpp
    src/Game.cpp
    src/InputManager.cpp
    src/Level.cpp
    src/LevelLoader.cpp
    src/LevelSession.cpp
    src/Platform.cpp
    src/Player.cpp
    src/PlayScene.cpp
    src/PowerUp.cpp
    src/SaveManager.cpp
    src/Sprites.cpp
    src/TileSet.cpp
    src/TitleScene.cpp
    src/UI.cpp
    src/World.cpp
)

add_library(pocketdash_core STATIC ${POCKETDASH_CORE_SOURCES})
target_include_directories(pocketdash_core PUBLIC src)
target_link_libraries(pocketdash_core PUBLIC pd_sdl)
target_compile_definitions(pocketdash_core PUBLIC
    POCKETDASH_VERSION="${PROJECT_VERSION}")

if(POCKETDASH_R36S)
    target_compile_definitions(pocketdash_core PUBLIC POCKETDASH_R36S=1)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        target_compile_options(pocketdash_core PUBLIC -mcpu=cortex-a35)
    endif()
endif()

if(MSVC)
    target_compile_options(pocketdash_core PRIVATE /W4)
else()
    target_compile_options(pocketdash_core PRIVATE -Wall -Wextra)
endif()

# GCC 8 and older need an extra library for std::filesystem.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(pocketdash_core PUBLIC stdc++fs)
endif()

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
add_executable(pocketdash src/main.cpp)
target_link_libraries(pocketdash PRIVATE pocketdash_core)
if(TARGET SDL2::SDL2main)
    target_link_libraries(pocketdash PRIVATE SDL2::SDL2main)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(POCKETDASH_BUILD_TESTS)
    enable_testing()
    add_executable(pocketdash_tests tests/test_main.cpp)
    target_link_libraries(pocketdash_tests PRIVATE pocketdash_core)
    # Lets the tests find assets/levels/*.lvl in the source tree.
    target_compile_definitions(pocketdash_tests PRIVATE POCKETDASH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(TARGET SDL2::SDL2main)
        target_link_libraries(pocketdash_tests PRIVATE SDL2::SDL2main)
    endif()

    add_test(NAME unit_tests COMMAND pocketdash_tests)

    # Loads every shipped level through the runtime data path.
    add_test(NAME level_files COMMAND pocketdash --check-levels)
    set_tests_properties(level_files PROPERTIES ENVIRONMENT "POCKETDASH_DATA=${CMAKE_CURRENT_SOURCE_DIR}")

    # Runs the real game loop headless with scripted input.
    add_test(NAME smoke_test COMMAND pocketdash --smoke-test)
    set_tests_properties(smoke_test PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;POCKETDASH_DATA=${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Install: produces a self-contained folder that can be copied to the
# handheld, e.g.  cmake --install build --prefix dist/PocketDash
# ---------------------------------------------------------------------------
install(TARGETS pocketdash RUNTIME DESTINATION .)
install(DIRECTORY assets config DESTINATION .)
install(DIRECTORY DESTINATION save)
install(PROGRAMS dist/PocketDash.sh DESTINATION .)
