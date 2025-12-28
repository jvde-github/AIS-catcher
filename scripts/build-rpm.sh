#!/bin/bash
#
# build-rpm.sh - Build AIS-catcher RPM package
#
# Usage: ./build-rpm.sh [-o output_dir] [-j jobs] [--skip-deps]
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
    pwd
}

readonly PROJECT_ROOT="$(find_project_root)"
readonly PACKAGE_NAME="ais-catcher"
readonly MAINTAINER="jvde-github <jvde-github@gmail.com>"
readonly DESCRIPTION="AIS-catcher - A multi-platform AIS receiver"
readonly HOMEPAGE="https://github.com/jvde-github/AIS-catcher"
readonly LICENSE="GPL-3.0"

# Directories
readonly BUILD_DIR="${PROJECT_ROOT}/build"
readonly RPM_ROOT="${BUILD_DIR}/rpm-pkg"
readonly LIB_INSTALL_DIR="/usr/lib64/${PACKAGE_NAME}"

# Source repositories
readonly RTLSDR_REPO="https://gitea.osmocom.org/sdr/rtl-sdr.git"
readonly HYDRASDR_REPO="https://github.com/hydrasdr/rfone_host.git"
readonly NMEA2000_REPO="https://github.com/ttlappalainen/NMEA2000.git"
readonly AIRSPY_REPO="https://github.com/airspy/airspyone_host.git"
readonly AIRSPYHF_REPO="https://github.com/airspy/airspyhf.git"

# Build dependencies (for dnf)
readonly BUILD_DEPS=(
    gcc-c++
    cmake
    make
    git
    pkg-config
    rpm-build
    patchelf
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
    export DEBIAN_FRONTEND=noninteractive
    
    if [[ $EUID -eq 0 ]]; then
        dnf install -y "${BUILD_DEPS[@]}"
    else
        sudo dnf install -y "${BUILD_DEPS[@]}"
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

build_airspy() {
    log "Building airspy library..."
    local src_dir="${BUILD_DIR}/airspyone_host"
    local install_dir="${BUILD_DIR}/local"
    
    git clone --depth 1 "${AIRSPY_REPO}" "${src_dir}"
    
    cmake -S "${src_dir}" -B "${src_dir}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install_dir}"
    
    cmake --build "${src_dir}/build" -j "${JOBS}"
    cmake --install "${src_dir}/build"
}

build_airspyhf() {
    log "Building airspyhf library..."
    local src_dir="${BUILD_DIR}/airspyhf"
    local install_dir="${BUILD_DIR}/local"
    
    git clone --depth 1 "${AIRSPYHF_REPO}" "${src_dir}"
    
    cmake -S "${src_dir}" -B "${src_dir}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install_dir}"
    
    cmake --build "${src_dir}/build" -j "${JOBS}"
    cmake --install "${src_dir}/build"
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
    
    export PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${install_dir}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
    export CMAKE_PREFIX_PATH="${install_dir}:${CMAKE_PREFIX_PATH:-}"
    export LD_LIBRARY_PATH="${install_dir}/lib:${install_dir}/lib64:${LD_LIBRARY_PATH:-}"
    
    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}/aiscatcher" \
        -DCMAKE_BUILD_TYPE=Release \
        -DNMEA2000_PATH="${BUILD_DIR}" \
        -DCMAKE_INSTALL_RPATH="${LIB_INSTALL_DIR}" \
        -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
        -DRUN_NUMBER="${run_num}"
    
    cmake --build "${BUILD_DIR}/aiscatcher" -j "${JOBS}"
    
    # Strip invalid RPATHs from binary, keep only the install path
    if command -v patchelf &>/dev/null; then
        log "Fixing RPATH with patchelf..."
        patchelf --set-rpath "${LIB_INSTALL_DIR}" "${BUILD_DIR}/aiscatcher/AIS-catcher"
    elif command -v chrpath &>/dev/null; then
        log "Fixing RPATH with chrpath..."
        chrpath -r "${LIB_INSTALL_DIR}" "${BUILD_DIR}/aiscatcher/AIS-catcher"
    fi
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
    
    # Convert to RPM version format: remove 'v' prefix, replace all '-' with '_'
    echo "${version}" | sed 's/^v//' | sed 's/-/_/g'
}

get_rpm_arch() {
    local arch=$(uname -m)
    case "${arch}" in
        x86_64)  echo "x86_64" ;;
        aarch64) echo "aarch64" ;;
        armv7l)  echo "armv7hl" ;;
        *)       echo "${arch}" ;;
    esac
}

extract_runtime_deps() {
    log "Extracting runtime dependencies..."
    local binary="${BUILD_DIR}/aiscatcher/AIS-catcher"
    local deps=""
    
    # Libraries we package ourselves (exclude from deps)
    local bundled_pattern='librtlsdr|libhydrasdr|libairspy'
    
    # Map library names to RPM package names
    while read -r lib; do
        [[ -z "${lib}" ]] && continue
        
        local pkg=""
        case "${lib}" in
            libhackrf*)    pkg="hackrf-libs" ;;
            libzmq*)       pkg="zeromq" ;;
            libssl*)       pkg="openssl-libs" ;;
            libz.*)        pkg="zlib" ;;
            libsqlite3*)   pkg="sqlite-libs" ;;
            libusb-1*)     pkg="libusb1" ;;
            libpq*)        pkg="postgresql-libs" ;;
        esac
        
        if [[ -n "${pkg}" && "${deps}" != *"${pkg}"* ]]; then
            deps="${deps}${deps:+, }${pkg}"
        fi
    done < <(ldd "${binary}" 2>/dev/null | grep -vE "${bundled_pattern}" | awk '{print $1}')
    
    echo "${deps}"
}

create_rpm_structure() {
    log "Creating RPM build structure..."
    
    local install_dir="${BUILD_DIR}/local"
    
    # Create RPM directories
    mkdir -p "${RPM_ROOT}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    
    # We'll let the spec file handle the install, just prepare the files
    # in a staging area that the spec can reference
    local staging="${RPM_ROOT}/staging"
    mkdir -p "${staging}/usr/bin"
    mkdir -p "${staging}${LIB_INSTALL_DIR}"
    mkdir -p "${staging}/usr/share/${PACKAGE_NAME}"
    mkdir -p "${staging}/etc/AIS-catcher/plugins"
    
    # Install binary
    install -m 755 "${BUILD_DIR}/aiscatcher/AIS-catcher" "${staging}/usr/bin/"
    
    # Install plugins
    if [[ -d "${PROJECT_ROOT}/plugins" ]]; then
        cp -r "${PROJECT_ROOT}/plugins/"* "${staging}/etc/AIS-catcher/plugins/"
    fi
    
    # Install udev rules
    cp "${BUILD_DIR}/rtl-sdr/rtl-sdr.rules" "${staging}/usr/share/${PACKAGE_NAME}/"
    cp "${BUILD_DIR}/rfone_host/hydrasdr-tools/51-hydrasdr.rules" "${staging}/usr/share/${PACKAGE_NAME}/"
    
    # Install bundled libraries
    for lib in librtlsdr libhydrasdr libairspy libairspyhf; do
        cp -P "${install_dir}/lib/"${lib}.so* "${staging}${LIB_INSTALL_DIR}/" 2>/dev/null || \
        cp -P "${install_dir}/lib64/"${lib}.so* "${staging}${LIB_INSTALL_DIR}/" 2>/dev/null || true
    done
}

create_spec_file() {
    local full_version="$1"
    local arch="$2"
    local deps="$3"
    
    # Extract base version (e.g., 0.66) and release (e.g., 0_g9b2edd7)
    local version="${full_version%%_*}"
    local release="${full_version#*_}"
    
    # If no underscore, use "1" as release
    if [[ "${release}" == "${version}" ]]; then
        release="1"
    else
        release=$(echo "${release}" | sed 's/_/./g')
    fi
    
    local staging="${RPM_ROOT}/staging"
    
    cat > "${RPM_ROOT}/SPECS/${PACKAGE_NAME}.spec" << EOF
Name:           ${PACKAGE_NAME}
Version:        ${version}
Release:        ${release}%{?dist}
Summary:        ${DESCRIPTION}
License:        ${LICENSE}
URL:            ${HOMEPAGE}
Requires:       ${deps}

# Disable debug package and build-id links
%global debug_package %{nil}
%define _build_id_links none

%description
AIS-catcher is a lightweight AIS receiver for RTL-SDR and other
SDR hardware. It supports various output formats and can feed
data to multiple destinations.

%install
cp -a ${staging}/* %{buildroot}/

%post
/sbin/ldconfig
if [ -d /etc/udev/rules.d ]; then
    cp /usr/share/${PACKAGE_NAME}/rtl-sdr.rules /etc/udev/rules.d/ 2>/dev/null || true
    cp /usr/share/${PACKAGE_NAME}/51-hydrasdr.rules /etc/udev/rules.d/ 2>/dev/null || true
    udevadm control --reload-rules 2>/dev/null || true
fi

%postun
/sbin/ldconfig

%files
/usr/bin/AIS-catcher
${LIB_INSTALL_DIR}
/usr/share/${PACKAGE_NAME}
%config(noreplace) /etc/AIS-catcher
EOF
}

build_package() {
    local version arch deps
    
    version=$(get_version)
    arch=$(get_rpm_arch)
    deps=$(extract_runtime_deps)
    
    log "Package version: ${version}"
    log "Architecture: ${arch}"
    log "Dependencies: ${deps}"
    
    create_rpm_structure
    create_spec_file "${version}" "${arch}" "${deps}"
    
    log "Building RPM package..."
    
    rpmbuild -bb \
        --define "_topdir ${RPM_ROOT}" \
        --define "_arch ${arch}" \
        --buildroot "${RPM_ROOT}/BUILDROOT" \
        "${RPM_ROOT}/SPECS/${PACKAGE_NAME}.spec"
    
    # Copy output
    local rpm_file=$(find "${RPM_ROOT}/RPMS" -name "*.rpm" -type f | head -1)
    if [[ -n "${rpm_file}" ]]; then
        cp "${rpm_file}" "${OUTPUT_DIR}/${PACKAGE_NAME}.rpm"
        log "Package created: ${OUTPUT_DIR}/${PACKAGE_NAME}.rpm"
        rpm -qip "${OUTPUT_DIR}/${PACKAGE_NAME}.rpm"
    else
        error "RPM package not found in ${RPM_ROOT}/RPMS"
    fi
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
    build_airspy
    build_airspyhf
    build_nmea2000
    build_aiscatcher
    build_package
    
    log "Build completed successfully!"
}

main "$@"