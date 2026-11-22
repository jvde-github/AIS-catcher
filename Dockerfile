FROM debian:trixie-slim

ARG RUN_NUMBER=0
ENV RUN_NUMBER=${RUN_NUMBER}
ARG TARGETARCH

# CI puts this run's packages in scripts/pkg, mounted for this step only; without them the last Edge release is installed
RUN --mount=type=bind,source=scripts,target=/tmp/scripts \
    set -e; \
    apt-get update; apt-get upgrade -y; \
    apt-get install -y --no-install-recommends ca-certificates; \
    arch=${TARGETARCH:-$(dpkg --print-architecture)}; \
    if [ "$arch" = arm ]; then arch=armhf; fi; \
    pkg=/tmp/scripts/pkg/ais-catcher_debian_trixie_${arch}.deb; \
    if [ -f "$pkg" ]; then \
        bash /tmp/scripts/aiscatcher-install --package-file "$pkg" --no-systemd --no-user; \
    else \
        apt-get install -y --no-install-recommends curl; \
        bash /tmp/scripts/aiscatcher-install --package --no-systemd --no-user; \
        apt-get remove -y curl; apt-get autoremove -y; \
    fi; \
    rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["/usr/bin/AIS-catcher"]
