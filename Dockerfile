# -------------------
# The build container
# -------------------
FROM alpine:latest AS build

RUN apk upgrade --no-cache
RUN apk add --no-cache build-base librtlsdr-dev

COPY . /root/AIS-catcher

RUN cd /root/AIS-catcher && \
  make rtl-only && \
  make install

# -------------------------
# The application container
# -------------------------
FROM alpine:latest

RUN apk upgrade --no-cache
RUN apk add --no-cache libusb librtlsdr libstdc++ libgcc

COPY --from=build /root/AIS-catcher/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
