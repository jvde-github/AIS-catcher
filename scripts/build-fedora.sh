#!/bin/bash
#
# build-fedora.sh - Build AIS-catcher RPM package (Fedora)
#
# Usage: ./build-fedora.sh [-o output_dir] [-j jobs] [--skip-deps]
#
# Environment: RUN_NUMBER - Build number for versioning (default: 0)
#

set -euo pipefail

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
readonly LIB_INSTALL_DIR="/usr/lib/${PACKAGE_NAME}"

# Directories
readonly BUILD_DIR="${PROJECT_ROOT}/build"

# Source repositories
readonly RTLSDR_REPO="https://gitea.osmocom.org/sdr/rtl-sdr.git"
readonly HYDRASDR_REPO="https://github.com/hydrasdr/hydrasdr-host.git"
readonly NMEA2000_REPO="https://github.com/ttlappalainen/NMEA2000.git"

# Build dependencies (for dnf)
readonly BUILD_DEPS=(
    gcc-c++
    make
    cmake
    git
    pkgconf-pkg-config
    rpm-build
    airspyone_host-devel
    airspyhf-devel
    hackrf-devel
    zeromq-devel
    openssl-devel
    zlib-devel
    sqlite-devel
    libusb1-devel
    libpq-devel
)

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2; }
error() { log "ERROR: $*"; exit 1; }

install_build_deps() {
    log "Installing build dependencies..."

    if [[ $EUID -eq 0 ]]; then
        dnf -y install "${BUILD_DEPS[@]}"
    else
        sudo dnf -y install "${BUILD_DEPS[@]}"
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
    local src_dir="${BUILD_DIR}/hydrasdr-host"
    local install_dir="${BUILD_DIR}/local"

    git clone --depth 1 "${HYDRASDR_REPO}" "${src_dir}"

    # Use pkg-config to find libusb (more reliable than manual search)
    local libusb_libdir
    libusb_libdir=$(pkg-config --variable=libdir libusb-1.0 2>/dev/null || true)

    local cmake_args="-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${install_dir}"

    # If pkg-config found libusb, help CMake find it
    if [[ -n "${libusb_libdir}" ]]; then
        cmake_args="${cmake_args} -DLIBUSB_LIBRARIES=${libusb_libdir}/libusb-1.0.so"
        cmake_args="${cmake_args} -DLIBUSB_INCLUDE_DIRS=$(pkg-config --variable=includedir libusb-1.0)/libusb-1.0"
    fi

    cmake -S "${src_dir}/libhydrasdr" -B "${src_dir}/libhydrasdr/build" ${cmake_args}

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

    # Allow git to work with mounted directories in Docker
    git config --global --add safe.directory '*' 2>/dev/null || true

    # Set up environment to find locally built libraries.
    # Fedora's GNUInstallDirs installs to lib64 on 64-bit; cover both.
    export PKG_CONFIG_PATH="${install_dir}/lib64/pkgconfig:${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export CMAKE_PREFIX_PATH="${install_dir}:${CMAKE_PREFIX_PATH:-}"
    export LD_LIBRARY_PATH="${install_dir}/lib64:${install_dir}/lib:${LD_LIBRARY_PATH:-}"

    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}/aiscatcher" \
        -DCMAKE_BUILD_TYPE=Release \
        -DNMEA2000_PATH="${BUILD_DIR}" \
        -DRPATH_LIBRARY_DIR="${LIB_INSTALL_DIR}" \
        -DRUN_NUMBER="${run_num}"

    cmake --build "${BUILD_DIR}/aiscatcher" -j "${JOBS}"
}

get_version() {
    local binary="${BUILD_DIR}/aiscatcher/AIS-catcher"

    if [[ ! -f "${binary}" ]]; then
        error "AIS-catcher binary not found at ${binary}"
    fi

    if [[ ! -x "${binary}" ]]; then
        error "AIS-catcher binary is not executable: ${binary}"
    fi

    local version
    version=$("${binary}" -h build 2>&1) || error "AIS-catcher failed to run: ${version}"

    if [[ -z "${version}" || "${version}" == *"error"* || "${version}" == *"Error"* ]]; then
        error "Failed to extract version from AIS-catcher binary. Output: ${version}"
    fi

    log "Extracted version: ${version}"

    # RPM version format: no 'v' prefix, no dashes (tilde sorts as pre-release, like Debian)
    echo "${version}" | sed 's/^v//' | tr '-' '~'
}

generate_spec() {
    local version="$1"
    local spec_file="$2"
    log "Generating RPM spec for version ${version}..."

    cat > "${spec_file}" << EOF
# Binary is built externally by build-fedora.sh before rpmbuild
%global debug_package %{nil}
%global _build_id_links none
%global __brp_check_rpaths %{nil}
# Bundled private libs under /usr/lib/ais-catcher: keep them out of the
# automatic Provides/Requires so dnf resolves against distro packages only
%global __provides_exclude ^(librtlsdr|libhydrasdr).*\$
%global __requires_exclude ^(librtlsdr|libhydrasdr).*\$

Name:           ${PACKAGE_NAME}
Version:        ${version}
Release:        1%{?dist}
Summary:        AIS-catcher - A multi-platform AIS receiver
License:        GPL-3.0-or-later
URL:            https://github.com/jvde-github/AIS-catcher

%description
AIS-catcher is a lightweight AIS receiver for RTL-SDR and other
SDR hardware. It supports various output formats and can feed
data to multiple destinations.

%install
install -D -m 755 ${BUILD_DIR}/aiscatcher/AIS-catcher %{buildroot}/usr/bin/AIS-catcher

mkdir -p %{buildroot}/etc/AIS-catcher/plugins
install -m 644 ${PROJECT_ROOT}/plugins/* %{buildroot}/etc/AIS-catcher/plugins/

install -D -m 644 ${BUILD_DIR}/rtl-sdr/rtl-sdr.rules %{buildroot}/usr/share/ais-catcher/rtl-sdr.rules
install -D -m 644 ${BUILD_DIR}/hydrasdr-host/hydrasdr-tools/51-hydrasdr.rules %{buildroot}/usr/share/ais-catcher/51-hydrasdr.rules

mkdir -p %{buildroot}${LIB_INSTALL_DIR}
find ${BUILD_DIR}/local -name 'librtlsdr.so*' -exec cp -a {} %{buildroot}${LIB_INSTALL_DIR}/ \\;
find ${BUILD_DIR}/local -name 'libhydrasdr.so*' -exec cp -a {} %{buildroot}${LIB_INSTALL_DIR}/ \\;

%files
/usr/bin/AIS-catcher
%dir /etc/AIS-catcher
%dir /etc/AIS-catcher/plugins
%config(noreplace) /etc/AIS-catcher/plugins/*
/usr/share/ais-catcher/
${LIB_INSTALL_DIR}/

%pre
echo "*******************************************************************" >&2
echo "* AIS-catcher is distributed under the GNU GPL v3 license.        *" >&2
echo "* This software is research and experimental software.            *" >&2
echo "* It is not intended for mission-critical use.                    *" >&2
echo "* Use it at your own risk and responsibility.                     *" >&2
echo "* It is your responsibility to ensure the legality of using       *" >&2
echo "* this software in your region.                                   *" >&2
echo "*******************************************************************" >&2

%post
if [ -d /etc/udev/rules.d ]; then
    cp -f /usr/share/ais-catcher/rtl-sdr.rules /etc/udev/rules.d/rtl-sdr.rules 2>/dev/null || :
    cp -f /usr/share/ais-catcher/51-hydrasdr.rules /etc/udev/rules.d/51-hydrasdr.rules 2>/dev/null || :
fi
udevadm control --reload-rules >/dev/null 2>&1 || :
udevadm trigger --subsystem-match=usb >/dev/null 2>&1 || :

%postun
if [ \$1 -eq 0 ]; then
    rm -f /etc/udev/rules.d/rtl-sdr.rules /etc/udev/rules.d/51-hydrasdr.rules
fi
EOF
}

build_package() {
    local version arch rpm_top spec_file rpm_file output_file

    version=$(get_version)
    arch=$(uname -m)

    log "Package version: ${version}"
    log "Architecture: ${arch}"

    rpm_top="${BUILD_DIR}/rpmbuild"
    spec_file="${rpm_top}/SPECS/${PACKAGE_NAME}.spec"
    mkdir -p "${rpm_top}/SPECS" "${rpm_top}/RPMS" "${rpm_top}/BUILD" "${rpm_top}/BUILDROOT"

    generate_spec "${version}" "${spec_file}"

    log "Building package with rpmbuild..."
    rpmbuild --define "_topdir ${rpm_top}" -bb "${spec_file}"

    rpm_file=$(find "${rpm_top}/RPMS" -name "${PACKAGE_NAME}-${version}-1.*.rpm" | head -n 1)
    [[ -n "${rpm_file}" ]] || error "Built RPM not found under ${rpm_top}/RPMS"

    output_file="${OUTPUT_DIR}/ais-catcher.rpm"
    mv "${rpm_file}" "${output_file}"

    # Verify package
    log "Verifying package..."
    rpm -qip "${output_file}"
    rpm -qlp "${output_file}"
    rpm -qp --requires "${output_file}"

    log "Package created: ${output_file}"
}

main() {
    local JOBS SKIP_DEPS

    JOBS=$(nproc)
    OUTPUT_DIR="${PROJECT_ROOT}"
    SKIP_DEPS=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -j|--jobs)     JOBS="$2"; shift 2 ;;
            -o|--output)   OUTPUT_DIR="$2"; shift 2 ;;
            --skip-deps)   SKIP_DEPS=true; shift ;;
            *) error "Unknown option: $1" ;;
        esac
    done

    export JOBS OUTPUT_DIR

    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}/local" "${OUTPUT_DIR}"

    [[ "${SKIP_DEPS}" == false ]] && install_build_deps

    build_rtlsdr
    build_hydrasdr
    build_nmea2000
    build_aiscatcher
    build_package

    log "Build completed successfully!"
}

main "$@"
