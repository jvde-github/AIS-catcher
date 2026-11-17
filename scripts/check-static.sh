#!/bin/sh
#
# check-static.sh - Build a minimal, fully static AIS-catcher on musl and run it
#
# -DMINIMAL=ON -DSTATIC=ON on Alpine, with rtl-sdr built from source as a static
# library: one binary without dependencies, for small devices and any Linux.
# Fails when the binary still needs a shared library.
#
# Usage (Alpine container, repository mounted at /src):
#   docker run --rm -v "$PWD":/src:ro alpine:3.21 sh /src/scripts/check-static.sh
#

set -eu

SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="/tmp/build-static"
RTLSDR_REPO="https://gitea.osmocom.org/sdr/rtl-sdr.git"

apk add -q build-base cmake linux-headers git pkgconf libusb-dev >/dev/null

git clone -q --depth 1 "$RTLSDR_REPO" /tmp/rtl-sdr
cmake -S /tmp/rtl-sdr -B /tmp/rtl-sdr/build -DCMAKE_BUILD_TYPE=Release \
    -DINSTALL_UDEV_RULES=OFF -DDETACH_KERNEL_DRIVER=ON >/dev/null
cmake --build /tmp/rtl-sdr/build -j"$(nproc)" >/dev/null
cmake --install /tmp/rtl-sdr/build >/dev/null

rm -rf "$BUILD_DIR"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DMINIMAL=ON -DSTATIC=ON -DWARNINGS_AS_ERRORS=ON \
    -DAIRSPY=OFF -DAIRSPYHF=OFF -DHACKRF=OFF -DHYDRASDR=OFF -DSDRPLAY=OFF
cmake --build "$BUILD_DIR" -j"$(nproc)"

if readelf -d "$BUILD_DIR/AIS-catcher" | grep -q NEEDED; then
    echo "not static:"; readelf -d "$BUILD_DIR/AIS-catcher" | grep NEEDED
    exit 1
fi
strip -o /tmp/AIS-catcher "$BUILD_DIR/AIS-catcher"
echo "static binary, $(stat -c %s /tmp/AIS-catcher) bytes stripped"
/tmp/AIS-catcher -h 2>&1 | head -1
