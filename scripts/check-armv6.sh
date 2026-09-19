#!/bin/bash
#
# check-armv6.sh - Compile and link AIS-catcher as Raspberry Pi OS 32-bit does
#
# Raspberry Pi OS compiles armv6 code on every Pi; Debian's armhf compiler
# defaults to armv7, so the armhf packages never see armv6-only failures such
# as 64-bit atomics that need libatomic. Cross-compiles, no emulation.
#
# Usage (Debian container, repository mounted at /src):
#   docker run --rm -v "$PWD":/src:ro debian:trixie bash /src/scripts/check-armv6.sh
#

set -euo pipefail

readonly SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_DIR="/tmp/build-armv6"
readonly TRIPLET="arm-linux-gnueabihf"

dpkg --add-architecture armhf
apt-get update -qq
apt-get install -y -qq crossbuild-essential-armhf cmake make pkgconf \
    libssl-dev:armhf zlib1g-dev:armhf libsqlite3-dev:armhf libpq-dev:armhf \
    librtlsdr-dev:armhf libzmq3-dev:armhf libusb-1.0-0-dev:armhf >/dev/null

# CMakeLists.txt replaces CMAKE_CXX_FLAGS, so the target flags ride on the compiler
for tool in gcc g++; do
    printf '#!/bin/sh\nexec %s-%s -march=armv6+fp -marm "$@"\n' "$TRIPLET" "$tool" > "/usr/local/bin/armv6-$tool"
    chmod +x "/usr/local/bin/armv6-$tool"
done

export PKG_CONFIG_LIBDIR="/usr/lib/${TRIPLET}/pkgconfig:/usr/share/pkgconfig"

rm -rf "$BUILD_DIR"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER=armv6-gcc -DCMAKE_CXX_COMPILER=armv6-g++ \
    -DWARNINGS_AS_ERRORS=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"

"${TRIPLET}-readelf" -d "$BUILD_DIR/AIS-catcher" | grep NEEDED
