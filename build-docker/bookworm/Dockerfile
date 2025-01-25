FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/New_York

RUN apt-get update && apt-get upgrade -y

COPY . /app
WORKDIR /app

RUN ./scripts/build.sh "cmake build-essential pkg-config git" ais-catcher amd64 1.0 ""

CMD ["cp", "ais-catcher.deb", "/output/"]
