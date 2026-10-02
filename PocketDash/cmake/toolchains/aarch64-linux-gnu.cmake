# Cross-compilation toolchain for the R36S (RK3326, Cortex-A35, ArkOS aarch64).
#
# Usage:
#   cmake -B build-r36s -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
#         -DPOCKETDASH_R36S=ON -DPOCKETDASH_SYSROOT=/path/to/arkos-sysroot
#
# POCKETDASH_SYSROOT is optional. Without it, the compiler's default search
# paths are used, which works with Debian/Ubuntu multiarch packages
# (e.g. `apt install libsdl2-dev:arm64 ...`).

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(POCKETDASH_CROSS_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross compiler prefix")
set(CMAKE_C_COMPILER   ${POCKETDASH_CROSS_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${POCKETDASH_CROSS_PREFIX}g++)

if(DEFINED ENV{POCKETDASH_SYSROOT} AND NOT POCKETDASH_SYSROOT)
    set(POCKETDASH_SYSROOT $ENV{POCKETDASH_SYSROOT})
endif()

if(DEFINED ENV{PKG_CONFIG_LIBDIR})
    # Caller configured pkg-config explicitly (e.g. SDL libraries extracted
    # from .deb files into a partial sysroot) - leave it alone.
elseif(POCKETDASH_SYSROOT)
    set(CMAKE_SYSROOT ${POCKETDASH_SYSROOT})
    set(CMAKE_FIND_ROOT_PATH ${POCKETDASH_SYSROOT})
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${POCKETDASH_SYSROOT})
    set(ENV{PKG_CONFIG_LIBDIR}
        "${POCKETDASH_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${POCKETDASH_SYSROOT}/usr/lib/pkgconfig:${POCKETDASH_SYSROOT}/usr/share/pkgconfig")
else()
    # Debian/Ubuntu multiarch layout.
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
