#!/bin/bash
# Builds an R36S/ArkOS-ready aarch64 package using Docker.
#
#   tools/build-arkos.sh            -> dist/PocketDash/  (copy to /roms/ports/)
#
# Requires Docker. Works on x86_64 Linux, macOS and Windows (WSL2) hosts:
# no emulation is needed because the container cross-compiles.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="pocketdash-arkos-build"

# Set BASE_IMAGE=mirror.gcr.io/library/ubuntu:20.04 if Docker Hub rate-limits you.
docker build -t "$IMAGE" ${BASE_IMAGE:+--build-arg BASE_IMAGE="$BASE_IMAGE"} "$ROOT/tools/arkos-build"

docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/src" "$IMAGE" bash -c '
    set -e
    cmake -S /src -B /src/build-arkos \
        -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchains/aarch64-linux-gnu.cmake \
        -DPOCKETDASH_R36S=ON -DPOCKETDASH_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
    cmake --build /src/build-arkos -j"$(nproc)"
    aarch64-linux-gnu-strip /src/build-arkos/pocketdash
    rm -rf /src/dist/PocketDash
    cmake --install /src/build-arkos --prefix /src/dist/PocketDash
    file /src/dist/PocketDash/pocketdash

    # ArkOS ships glibc 2.30: fail loudly if we accidentally need newer symbols.
    newest=$(aarch64-linux-gnu-objdump -T /src/dist/PocketDash/pocketdash \
             | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sort -Vu | tail -1)
    echo "Newest glibc symbol required: $newest"
    if [ "$(printf "%s\nGLIBC_2.30\n" "$newest" | sort -V | tail -1)" != "GLIBC_2.30" ]; then
        echo "ERROR: binary requires $newest, newer than ArkOS (GLIBC_2.30)" >&2
        exit 1
    fi
'

# Release archive: PocketDash/ (game folder) + PocketDash.sh (Ports entry),
# laid out exactly as they go into /roms/ports/.
VERSION=$(sed -n "s/^project(PocketDash VERSION \([0-9.]*\).*/\1/p" "$ROOT/CMakeLists.txt")
ZIP="$ROOT/dist/PocketDash-$VERSION-arkos.zip"
STAGE="$(mktemp -d)"
cp -r "$ROOT/dist/PocketDash" "$STAGE/PocketDash"
cp "$ROOT/dist/PocketDash/PocketDash.sh" "$STAGE/PocketDash.sh"
rm -f "$ZIP"
(cd "$STAGE" && cmake -E tar cf "$ZIP" --format=zip PocketDash PocketDash.sh)
rm -rf "$STAGE"

echo
echo "Done: $ROOT/dist/PocketDash"
echo "Release archive: $ZIP (unzip it into /roms/ports/)"
echo "Copy dist/PocketDash/ to /roms/ports/PocketDash/ and"
echo "dist/PocketDash/PocketDash.sh to /roms/ports/PocketDash.sh"
