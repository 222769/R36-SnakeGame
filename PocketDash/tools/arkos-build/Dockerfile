# Cross-compilation environment for ArkOS on the R36S.
#
# ArkOS images are based on Ubuntu 19.10 (glibc 2.30). A binary built on a
# modern distro links against newer glibc symbols and refuses to start on
# the device, so we build inside Ubuntu 20.04 (GCC 9, SDL 2.0.10). The build
# script checks that the binary needs nothing newer than GLIBC_2.30.
#
# Used by tools/build-arkos.sh - see README.md.

# Override with --build-arg BASE_IMAGE=mirror.gcr.io/library/ubuntu:20.04
# if Docker Hub rate-limits you.
ARG BASE_IMAGE=ubuntu:20.04
FROM ${BASE_IMAGE}

# amd64 packages come from the normal mirror, arm64 from ports.ubuntu.com.
RUN sed -i 's/^deb /deb [arch=amd64] /' /etc/apt/sources.list \
 && printf '%s\n' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal main universe' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal-updates main universe' \
      > /etc/apt/sources.list.d/arm64.list \
 && dpkg --add-architecture arm64 \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates cmake make pkg-config file binutils-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        libsdl2-dev:arm64 libsdl2-image-dev:arm64 libsdl2-ttf-dev:arm64 libsdl2-mixer-dev:arm64 \
 && rm -rf /var/lib/apt/lists/*

ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig

WORKDIR /src
