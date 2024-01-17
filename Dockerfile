# -------------------
# The build container
# -------------------
FROM debian:bookworm-slim AS build

RUN apt-get update
RUN apt-get upgrade -y

RUN apt-get install git make gcc g++ cmake pkg-config -y
RUN apt-get install librtlsdr-dev libairspy-dev libhackrf-dev libairspyhf-dev libzmq3-dev libsoxr-dev zlib1g-dev libpq-dev libssl-dev -y

COPY . /root/AIS-catcher

RUN cd /root/AIS-catcher; git clone https://github.com/ttlappalainen/NMEA2000.git;
RUN cd /root/AIS-catcher/NMEA2000/src; g++ -O3 -c  N2kMsg.cpp  N2kStream.cpp N2kMessages.cpp   N2kTimer.cpp  NMEA2000.cpp  N2kGroupFunctionDefaultHandlers.cpp  N2kGroupFunction.cpp  -I.
RUN cd /root/AIS-catcher/NMEA2000/src; ar rcs libnmea2000.a *.o
RUN cd /root/AIS-catcher; mkdir build; cd build; cmake .. -DNMEA2000_PATH=/root/AIS-catcher/NMEA2000/src; make; make install

# -------------------------
# The application container
# -------------------------
FROM debian:bookworm-slim

RUN apt-get update
RUN apt-get upgrade -y

RUN git clone https://github.com/rtlsdrblog/rtl-sdr-blog && cd rtl-sdr-blog/ && dpkg-buildpackage -b --no-sign && cd .. && dpkg -i librtlsdr0_*.deb && dpkg -i librtlsdr-dev_*.deb && dpkg -i rtl-sdr_*.deb
RUN apt-get install libairspy0 libhackrf0 libairspyhf1 libzmq5 libsoxr0 libpq5 libz1 libssl3 -y

COPY --from=build /usr/local/bin/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
