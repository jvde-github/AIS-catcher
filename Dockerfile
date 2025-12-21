FROM debian:bookworm-slim

ARG RUN_NUMBER=0
ENV RUN_NUMBER=${RUN_NUMBER}

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends curl ca-certificates && \
    bash -c "$(curl -fsSL https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)" _ --package --no-systemd --no-user && \
    apt-get remove -y curl ca-certificates && apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["/usr/bin/AIS-catcher"]
