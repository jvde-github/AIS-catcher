# -------------------
# The build container
# -------------------
FROM debian:stretch-slim AS build

RUN apt-get update
RUN apt-get upgrade

RUN apt-get install git make gcc g++ cmake pkg-config -y
RUN apt-get install librtlsdr-dev libairspy-dev libairspyhf-dev libhackrf-dev libzmq3-dev

COPY . /root/AIS-catcher

RUN cd /root/AIS-catcher; mkdir build; cd build; cmake ..; make; make install

# -------------------------
# The application container
# -------------------------
FROM debian:stretch-slim

RUN apt-get update
RUN apt-get upgrade

RUN apt-get install librtlsdr0 libairspy0 libairspyhf1 libhackrf0 libzmq5

COPY --from=build /usr/local/bin/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
