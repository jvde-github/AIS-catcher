# -------------------
# The build container
# -------------------
FROM alpine:latest AS build

RUN apk upgrade --no-cache
RUN apk add --no-cache build-base librtlsdr-dev git make gcc g++ libusb-dev automake autoconf cmake util-linux musl-utils fftw-dev zeromq-dev

COPY . /root/AIS-catcher

RUN git clone https://github.com/airspy/airspyhf; cd airspyhf; mkdir build; cd build; cmake ../ -DINSTALL_UDEV_RULES=ON; make; make install; ldconfig /etc/ld.so.conf.d
RUN git clone https://github.com/airspy/airspyone_host; cd airspyone_host; mkdir build; cd build; cmake ../ -DINSTALL_UDEV_RULES=ON; make; make install; ldconfig /etc/ld.so.conf.d
RUN git clone https://github.com/greatscottgadgets/hackrf.git; cd hackrf/host; mkdir build; cd build; cmake ..; make; make install; ldconfig /etc/ld.so.conf.d
RUN cd /root/AIS-catcher; mkdir build; cd build; cmake ..; make; make install

# -------------------------
# The application container
# -------------------------
FROM alpine:latest

RUN apk upgrade --no-cache
RUN apk add --no-cache libusb librtlsdr libstdc++ libgcc libzmq

COPY --from=build /usr/local/lib/libairspyhf.so /usr/local/lib/libairspyhf.so
#COPY --from=build /usr/local/lib/libairspyhf.so.0 /usr/local/lib/libairspyhf.so.0

COPY --from=build /usr/local/lib/libairspy.so /usr/local/lib/libairspy.so
COPY --from=build /usr/local/lib/libairspy.so.0 /usr/local/lib/libairspy.so.0

COPY --from=build /usr/local/lib/libhackrf.so /usr/local/lib/libhackrf.so
COPY --from=build /usr/local/lib/libhackrf.so.0 /usr/local/lib/libhackrf.so.0

COPY --from=build /usr/local/bin/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
