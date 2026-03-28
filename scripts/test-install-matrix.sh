#!/bin/bash
#
# test-install-matrix.sh - Test AIS-catcher package installation across the build matrix
#
# Usage:
#   ./scripts/test-install-matrix.sh                        # run full matrix
#   ./scripts/test-install-matrix.sh debian trixie armhf   # single combination
#   ./scripts/test-install-matrix.sh debian trixie          # all arches for one distro/codename
#   ./scripts/test-install-matrix.sh "" "" armhf            # all armhf combos
#
# Requires: Docker with QEMU support (docker/setup-qemu-action or equivalent)

set -euo pipefail

SCRIPT_URL="https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install"
LOG_DIR="/tmp/ais-catcher-test-logs"
mkdir -p "${LOG_DIR}"

# Matrix (must match .github/workflows/build.yml)
DEBIAN_CODENAMES=(bullseye bookworm trixie)
UBUNTU_CODENAMES=(focal jammy noble plucky questing resolute)
ARCHS=(amd64 arm64 armhf)

# Filters from args (empty = no filter)
FILTER_DISTRO="${1:-}"
FILTER_CODENAME="${2:-}"
FILTER_ARCH="${3:-}"

PASS=0
FAIL=0
declare -a RESULTS=()

platform_for_arch() {
    case "$1" in
        armhf) echo "linux/arm/v7" ;;
        arm64) echo "linux/arm64" ;;
        amd64) echo "linux/amd64" ;;
    esac
}

run_test() {
    local distro=$1 codename=$2 arch=$3
    local platform label logfile
    platform=$(platform_for_arch "${arch}")
    label="${distro}:${codename}/${arch}"
    logfile="${LOG_DIR}/${distro}-${codename}-${arch}.log"

    printf "  %-36s " "${label}"

    if docker run --rm \
        --platform "${platform}" \
        "${distro}:${codename}" \
        bash -c "
            set -e
            apt-get update -qq
            apt-get install -y -qq curl
            bash <(curl -fsSL ${SCRIPT_URL}) --package --no-systemd --no-user
            AIS-catcher -l
        " > "${logfile}" 2>&1; then
        echo "PASS"
        PASS=$((PASS + 1))
        RESULTS+=("PASS  ${label}")
    else
        echo "FAIL  (log: ${logfile})"
        FAIL=$((FAIL + 1))
        RESULTS+=("FAIL  ${label}")
    fi
}

should_run() {
    local distro=$1 codename=$2 arch=$3
    [[ -n "${FILTER_DISTRO}"   && "${FILTER_DISTRO}"   != "${distro}"   ]] && return 1
    [[ -n "${FILTER_CODENAME}" && "${FILTER_CODENAME}" != "${codename}" ]] && return 1
    [[ -n "${FILTER_ARCH}"     && "${FILTER_ARCH}"     != "${arch}"     ]] && return 1
    return 0
}

# Setup QEMU for non-native arches.
# On Linux hosts this is required; Docker Desktop on Mac manages it automatically.
if [[ "$(uname -s)" == "Linux" ]]; then
    echo "Setting up QEMU..."
    docker run --rm --privileged multiarch/qemu-user-static --reset -p yes > /dev/null 2>&1 || true
fi
echo ""

echo "Running install tests..."
echo ""

for codename in "${DEBIAN_CODENAMES[@]}"; do
    for arch in "${ARCHS[@]}"; do
        should_run debian "${codename}" "${arch}" || continue
        run_test debian "${codename}" "${arch}"
    done
done

for codename in "${UBUNTU_CODENAMES[@]}"; do
    for arch in "${ARCHS[@]}"; do
        # Match matrix exclusion: focal/armhf not built
        [[ "${codename}" == "focal" && "${arch}" == "armhf" ]] && continue
        should_run ubuntu "${codename}" "${arch}" || continue
        run_test ubuntu "${codename}" "${arch}"
    done
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for r in "${RESULTS[@]}"; do
    echo "  ${r}"
done
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PASS: ${PASS}   FAIL: ${FAIL}   TOTAL: $((PASS + FAIL))"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

[[ ${FAIL} -eq 0 ]]
