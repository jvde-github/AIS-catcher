FROM debian:bookworm

ARG RUN_NUMBER=0
ENV RUN_NUMBER=${RUN_NUMBER}
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/New_York

RUN apt-get update &&  apt-get dist-upgrade -y && apt-get install -y build-essential

COPY . /app
WORKDIR /app

RUN ./scripts/build.sh "cmake build-essential pkg-config git" ais-catcher amd64 ""

CMD ["cp", "ais-catcher.deb", "/output/"]
