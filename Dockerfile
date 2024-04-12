# -------------------
# The build container
# -------------------
FROM debian:bookworm-slim AS build

RUN apt-get update
RUN apt-get upgrade -y

RUN apt-get install git make gcc g++ cmake pkg-config -y
RUN apt-get install libusb-1.0.0-dev libairspy-dev libhackrf-dev libairspyhf-dev libzmq3-dev libsoxr-dev zlib1g-dev libpq-dev libssl-dev -y

COPY . /root/AIS-catcher

RUN cd /root/AIS-catcher; git clone https://gitea.osmocom.org/sdr/rtl-sdr.git
RUN cd /root/AIS-catcher/rtl-sdr; mkdir build; cd build; cmake ../ -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON; make; make install; 
RUN cp /root/AIS-catcher/rtl-sdr/rtl-sdr.rules /etc/udev/rules.d/
RUN ldconfig

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

RUN apt-get install git make gcc g++ cmake pkg-config libusb-1.0-0-dev -y
RUN apt-get install libusb-1.0 librtlsdr0 libairspy0 libhackrf0 libairspyhf1 libzmq5 libsoxr0 libpq5 libz1 libssl3 -y

RUN cd /root; git clone https://github.com/osmocom/rtl-sdr.git
RUN cd /root/rtl-sdr; mkdir build; cd build; cmake ../ -DCMAKE_BUILD_TYPE=Release -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON; make; make install;
RUN cp /root/rtl-sdr/rtl-sdr.rules /etc/udev/rules.d/
RUN ldconfig

RUN apt-get remove git make gcc g++ cmake pkg-config libusb-1.0-0-dev -y
RUN  apt-get autoremove -y

COPY --from=build /usr/local/bin/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
