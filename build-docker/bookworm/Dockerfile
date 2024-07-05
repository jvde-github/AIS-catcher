FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/New_York

RUN apt-get update && apt-get install -y 

COPY . /app
WORKDIR /app

RUN ./build.me "cmake build-essential pkg-config" ais-catcher amd64 1.0 ""

CMD ["cp", "ais-catcher.deb", "/output/"]
