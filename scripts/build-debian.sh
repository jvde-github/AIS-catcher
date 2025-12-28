#!/bin/bash
#
# build-debian.sh - Build AIS-catcher Debian package
#
# Description:
#   Builds AIS-catcher from source and creates a Debian package using
#   standard Debian packaging tools and best practices.
#
# Usage:
#   ./build-debian.sh [OPTIONS]
#
# Options:
#   -a, --arch ARCH     Target architecture (default: auto-detect)
#   -j, --jobs N        Parallel build jobs (default: nproc)
#   -o, --output DIR    Output directory for .deb (default: current dir)
#   --skip-deps         Skip installing build dependencies (if already installed)
#   -h, --help          Show this help message
#
# Environment:
#   RUN_NUMBER          Build number for versioning (default: 0)
#
# Author: AIS-catcher Project
# License: GNU GPL v3

set -euo pipefail

################################################################################
# Configuration
################################################################################

# Detect project root (look for CMakeLists.txt)
find_project_root() {
    local dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    while [[ "$dir" != "/" ]]; do
        if [[ -f "$dir/CMakeLists.txt" ]]; then
            echo "$dir"
            return 0
        fi
        dir="$(dirname "$dir")"
    done
    # Fallback to current directory
    pwd
}

readonly PROJECT_ROOT="$(find_project_root)"
readonly PACKAGE_NAME="ais-catcher"
readonly MAINTAINER="jvde-github <jvde-github@gmail.com>"
readonly DESCRIPTION="AIS-catcher - A multi-platform AIS receiver"
readonly HOMEPAGE="https://github.com/jvde-github/AIS-catcher"
readonly LICENSE="GPL-3"

# Directories
readonly BUILD_DIR="${PROJECT_ROOT}/build"
readonly DEBIAN_ROOT="${BUILD_DIR}/debian-pkg"
readonly LIB_INSTALL_DIR="/usr/lib/${PACKAGE_NAME}"

# Source repositories
readonly RTLSDR_REPO="https://gitea.osmocom.org/sdr/rtl-sdr.git"
readonly HYDRASDR_REPO="https://github.com/hydrasdr/rfone_host.git"
readonly NMEA2000_REPO="https://github.com/ttlappalainen/NMEA2000.git"

# Build dependencies (for apt-get)
readonly BUILD_DEPS=(
    build-essential
    cmake
    git
    pkg-config
    fakeroot
    libairspy-dev
    libairspyhf-dev
    libhackrf-dev
    libzmq3-dev
    libssl-dev
    zlib1g-dev
    libsqlite3-dev
    libusb-1.0-0-dev
    libpq-dev
)

################################################################################
# Utility Functions
################################################################################

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

error() {
    log "ERROR: $*"
    exit 1
}

usage() {
    sed -n '/^# Usage:/,/^# Author:/p' "$0" | grep -v '^# Author:' | sed 's/^# //'
    exit 0
}

################################################################################
# Build Functions
################################################################################

install_build_deps() {
    log "Installing build dependencies..."
    
    if [[ $EUID -eq 0 ]]; then
        apt-get update -qq
        apt-get install -y -qq "${BUILD_DEPS[@]}"
    else
        sudo apt-get update -qq
        sudo apt-get install -y -qq "${BUILD_DEPS[@]}"
    fi
}

build_rtlsdr() {
    log "Building rtl-sdr library..."
    local src_dir="${BUILD_DIR}/rtl-sdr"
    local install_dir="${BUILD_DIR}/local"
    
    git clone --depth 1 "${RTLSDR_REPO}" "${src_dir}"
    
    cmake -S "${src_dir}" -B "${src_dir}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install_dir}" \
        -DINSTALL_UDEV_RULES=ON \
        -DDETACH_KERNEL_DRIVER=ON
    
    cmake --build "${src_dir}/build" -j "${JOBS}"
    cmake --install "${src_dir}/build"
}

build_hydrasdr() {
    log "Building libhydrasdr library..."
    local src_dir="${BUILD_DIR}/rfone_host"
    local install_dir="${BUILD_DIR}/local"
    
    git clone --depth 1 "${HYDRASDR_REPO}" "${src_dir}"
    
    cmake -S "${src_dir}/libhydrasdr" -B "${src_dir}/libhydrasdr/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install_dir}"
    
    cmake --build "${src_dir}/libhydrasdr/build" -j "${JOBS}"
    cmake --install "${src_dir}/libhydrasdr/build"
}

build_nmea2000() {
    log "Building NMEA2000 library..."
    local src_dir="${BUILD_DIR}/NMEA2000"
    
    git clone --depth 1 "${NMEA2000_REPO}" "${src_dir}"
    
    pushd "${src_dir}/src" > /dev/null
    g++ -O3 -c \
        N2kMsg.cpp \
        N2kStream.cpp \
        N2kMessages.cpp \
        N2kTimer.cpp \
        NMEA2000.cpp \
        N2kGroupFunctionDefaultHandlers.cpp \
        N2kGroupFunction.cpp \
        -I.
    ar rcs libnmea2000.a ./*.o
    popd > /dev/null
}

build_aiscatcher() {
    log "Building AIS-catcher..."
    local run_num="${RUN_NUMBER:-0}"
    local install_dir="${BUILD_DIR}/local"
    
    # Set up environment to find locally built libraries
    export PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export CMAKE_PREFIX_PATH="${install_dir}:${CMAKE_PREFIX_PATH:-}"
    export LD_LIBRARY_PATH="${install_dir}/lib:${LD_LIBRARY_PATH:-}"
    
    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}/aiscatcher" \
        -DCMAKE_BUILD_TYPE=Release \
        -DNMEA2000_PATH="${BUILD_DIR}" \
        -DRPATH_LIBRARY_DIR="${LIB_INSTALL_DIR}" \
        -DRUN_NUMBER="${run_num}"
    
    cmake --build "${BUILD_DIR}/aiscatcher" -j "${JOBS}"
}

get_version() {
    local binary="${BUILD_DIR}/aiscatcher/AIS-catcher"
    
    if [[ ! -x "${binary}" ]]; then
        error "AIS-catcher binary not found at ${binary}"
    fi
    
    local version
    version=$("${binary}" -h build 2>/dev/null || true)
    
    if [[ -z "${version}" ]]; then
        error "Failed to extract version from AIS-catcher binary"
    fi
    
    # Convert to Debian version format: remove 'v' prefix, replace first '-' with '~'
    echo "${version}" | sed 's/^v//' | sed 's/-/~/'
}

extract_runtime_deps() {
    log "Extracting runtime dependencies..."
    local binary="${BUILD_DIR}/aiscatcher/AIS-catcher"
    local deps=""
    
    # Libraries we package ourselves (exclude from deps)
    local bundled_pattern='librtlsdr|libhydrasdr'
    
    # Libraries we want as dependencies
    local dep_pattern='libairspy|libairspyhf|libhackrf|libzmq|libz\.|libssl|libusb-1\.0|libsqlite3|libpq'
    
    while read -r lib; do
        [[ -z "${lib}" ]] && continue
        
        # Extract library name and version for Debian package format
        local lib_name lib_version
        lib_name=$(echo "${lib}" | sed 's/\.so.*//')
        lib_version=$(echo "${lib}" | sed 's/.*\.so\.//')
        
        # Handle libraries that end with a digit (need hyphen before version)
        if [[ "${lib_name}" =~ [0-9]$ ]]; then
            deps="${deps}${deps:+, }${lib_name}-${lib_version}"
        else
            deps="${deps}${deps:+, }${lib_name}${lib_version}"
        fi
    done < <(ldd "${binary}" 2>/dev/null | grep -E "${dep_pattern}" | grep -vE "${bundled_pattern}" | awk '{print $1}')
    
    echo "${deps}"
}

################################################################################
# Package Creation
################################################################################

create_debian_structure() {
    log "Creating Debian package structure..."
    
    local pkg_dir="${DEBIAN_ROOT}"
    local install_dir="${BUILD_DIR}/local"
    
    # Standard directories
    mkdir -p "${pkg_dir}/DEBIAN"
    mkdir -p "${pkg_dir}/usr/bin"
    mkdir -p "${pkg_dir}/usr/share/${PACKAGE_NAME}"
    mkdir -p "${pkg_dir}/etc/AIS-catcher/plugins"
    mkdir -p "${pkg_dir}${LIB_INSTALL_DIR}"
    
    # Install binary
    install -m 755 "${BUILD_DIR}/aiscatcher/AIS-catcher" "${pkg_dir}/usr/bin/"
    
    # Install plugins
    if [[ -d "${PROJECT_ROOT}/plugins" ]]; then
        cp -r "${PROJECT_ROOT}/plugins/"* "${pkg_dir}/etc/AIS-catcher/plugins/"
    fi
    
    # Install udev rules to share directory (postinst will install them)
    cp "${BUILD_DIR}/rtl-sdr/rtl-sdr.rules" "${pkg_dir}/usr/share/${PACKAGE_NAME}/"
    cp "${BUILD_DIR}/rfone_host/hydrasdr-tools/51-hydrasdr.rules" "${pkg_dir}/usr/share/${PACKAGE_NAME}/"
    
    # Install bundled libraries from local build
    cp -P "${install_dir}/lib/"librtlsdr.so* "${pkg_dir}${LIB_INSTALL_DIR}/"
    cp -P "${install_dir}/lib/"libhydrasdr.so* "${pkg_dir}${LIB_INSTALL_DIR}/"
}

create_control_file() {
    local version="$1"
    local arch="$2"
    local deps="$3"
    
    cat > "${DEBIAN_ROOT}/DEBIAN/control" << EOF
Package: ${PACKAGE_NAME}
Version: ${version}
Architecture: ${arch}
Maintainer: ${MAINTAINER}
Description: ${DESCRIPTION}
 AIS-catcher is a lightweight AIS receiver for RTL-SDR and other
 SDR hardware. It supports various output formats and can feed
 data to multiple destinations.
Homepage: ${HOMEPAGE}
Section: hamradio
Priority: optional
Depends: ${deps}
EOF
}

create_preinst() {
    cat > "${DEBIAN_ROOT}/DEBIAN/preinst" << 'EOF'
#!/bin/bash
cat << 'NOTICE' >&2
*******************************************************************
*                                                                 *
* AIS-catcher is distributed under the GNU GPL v3 license.        *
* This software is research and experimental software.            *
* It is not intended for mission-critical use.                    *
* Use it at your own risk and responsibility.                     *
* It is your responsibility to ensure the legality of using       *
* this software in your region.                                   *
*                                                                 *
*******************************************************************
NOTICE
exit 0
EOF
    chmod 755 "${DEBIAN_ROOT}/DEBIAN/preinst"
}

create_postinst() {
    cat > "${DEBIAN_ROOT}/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

SHARE_DIR="/usr/share/ais-catcher"
UDEV_DIR="/etc/udev/rules.d"

install_udev_rules() {
    local rules_file="$1"
    local source="${SHARE_DIR}/${rules_file}"
    local target="${UDEV_DIR}/${rules_file}"
    
    [[ ! -f "${source}" ]] && return 0
    
    if [[ -f "${target}" ]]; then
        if command -v md5sum &>/dev/null; then
            local src_md5 tgt_md5
            src_md5=$(md5sum "${source}" | cut -d' ' -f1)
            tgt_md5=$(md5sum "${target}" | cut -d' ' -f1)
            [[ "${src_md5}" == "${tgt_md5}" ]] && return 0
        fi
        cp "${target}" "${target}.backup" 2>/dev/null || true
    fi
    
    cp "${source}" "${target}"
}

reload_udev() {
    if command -v udevadm &>/dev/null; then
        udevadm control --reload-rules 2>/dev/null || true
        udevadm trigger --subsystem-match=usb 2>/dev/null || true
    fi
}

case "$1" in
    configure)
        install_udev_rules "rtl-sdr.rules"
        install_udev_rules "51-hydrasdr.rules"
        reload_udev
        ldconfig
        ;;
esac

exit 0
EOF
    chmod 755 "${DEBIAN_ROOT}/DEBIAN/postinst"
}

create_postrm() {
    cat > "${DEBIAN_ROOT}/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e

case "$1" in
    purge)
        rm -f /etc/udev/rules.d/rtl-sdr.rules
        rm -f /etc/udev/rules.d/51-hydrasdr.rules
        rm -rf /etc/AIS-catcher
        ;;
    remove|upgrade|failed-upgrade|abort-install|abort-upgrade|disappear)
        ;;
esac

exit 0
EOF
    chmod 755 "${DEBIAN_ROOT}/DEBIAN/postrm"
}

build_package() {
    local version arch deps output_file
    
    version=$(get_version)
    arch="${TARGET_ARCH:-$(dpkg --print-architecture)}"
    deps=$(extract_runtime_deps)
    
    log "Package version: ${version}"
    log "Architecture: ${arch}"
    log "Dependencies: ${deps}"
    
    create_debian_structure
    create_control_file "${version}" "${arch}" "${deps}"
    create_preinst
    create_postinst
    create_postrm
    
    # Build package with fakeroot if available
    output_file="${OUTPUT_DIR}/${PACKAGE_NAME}_${version}_${arch}.deb"
    
    log "Building package: ${output_file}"
    
    if command -v fakeroot &>/dev/null; then
        fakeroot dpkg-deb --build "${DEBIAN_ROOT}" "${output_file}"
    else
        dpkg-deb --build "${DEBIAN_ROOT}" "${output_file}"
    fi
    
    # Verify package
    log "Verifying package..."
    dpkg-deb --info "${output_file}"
    
    log "Package created: ${output_file}"
}

################################################################################
# Main
################################################################################

main() {
    local JOBS TARGET_ARCH OUTPUT_DIR SKIP_DEPS
    
    # Defaults
    JOBS=$(nproc)
    TARGET_ARCH=""
    OUTPUT_DIR="${PROJECT_ROOT}"
    SKIP_DEPS=false
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -a|--arch)
                TARGET_ARCH="$2"
                shift 2
                ;;
            -j|--jobs)
                JOBS="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            -h|--help)
                usage
                ;;
            *)
                error "Unknown option: $1"
                ;;
        esac
    done
    
    # Export for subshells
    export JOBS TARGET_ARCH OUTPUT_DIR
    
    # Clean and create build directory
    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}/local"
    mkdir -p "${OUTPUT_DIR}"
    
    # Install dependencies (requires sudo if not root)
    if [[ "${SKIP_DEPS}" == false ]]; then
        install_build_deps
    fi
    
    # Build everything (no root required)
    build_rtlsdr
    build_hydrasdr
    build_nmea2000
    build_aiscatcher
    build_package
    
    log "Build completed successfully!"
}

main "$@"
