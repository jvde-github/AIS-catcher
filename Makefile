SRC = Tracking/Ships.cpp Library/N2K.cpp IO/N2KInterface.cpp Device/N2KsktCAN.cpp IO/N2KStream.cpp Application/Prometheus.cpp Application/Main.cpp Application/WebViewer.cpp Application/MapTiles.cpp IO/HTTPClient.cpp DBMS/PostgreSQL.cpp Tracking/DB.cpp Application/Config.cpp Application/Receiver.cpp IO/HTTPServer.cpp DSP/DSP.cpp JSON/JSONAIS.cpp JSON/Parser.cpp JSON/StringBuilder.cpp JSON/Keys.cpp Library/AIS.cpp IO/Network.cpp DSP/Model.cpp Library/NMEA.cpp Library/Utilities.cpp DSP/Demod.cpp Library/Message.cpp Device/UDP.cpp Device/ZMQ.cpp Device/RTLSDR.cpp Device/AIRSPYHF.cpp Device/SoapySDR.cpp Device/AIRSPY.cpp Device/FileRAW.cpp Device/FileWAV.cpp Device/SDRPLAY.cpp Device/RTLTCP.cpp Device/HACKRF.cpp Device/Serial.cpp Library/TCP.cpp Device/SpyServer.cpp JSON/JSON.cpp IO/Protocol.cpp IO/MsgOut.cpp Library/Logger.cpp Library/Basestation.cpp  Application/WebDB.cpp Library/Beast.cpp Library/ADSB.cpp
OBJ = Ships.o Main.o N2KStream.o N2K.o N2KInterface.o N2KsktCAN.o Prometheus.o Receiver.o Config.o WebViewer.o MapTiles.o HTTPClient.o PostgreSQL.o DB.o DSP.o AIS.o Model.o Utilities.o Network.o Demod.o Serial.o RTLSDR.o HTTPServer.o AIRSPYHF.o Keys.o AIRSPY.o Parser.o StringBuilder.o FileRAW.o FileWAV.o SDRPLAY.o NMEA.o RTLTCP.o HACKRF.o ZMQ.o UDP.o SoapySDR.o TCP.o Message.o SpyServer.o JSON.o JSONAIS.o Protocol.o MsgOut.o Logger.o Basestation.o WebDB.o Beast.o ADSB.o
INCLUDE = -I. -IJSON/ -IDBMS/ -ITracking/ -ILibrary/ -IDSP/ -IApplication/ -IIO/ 
CC = clang

override CFLAGS +=  -Ofast -std=c++11 -g -pg -Wno-sign-compare $(INCLUDE)
override LFLAGS += -lstdc++ -lpthread -g -pg -lm -o AIS-catcher

CFLAGS_RTL = -DHASRTLSDR $(shell pkg-config --cflags librtlsdr)
CFLAGS_AIRSPYHF = -DHASAIRSPYHF $(shell pkg-config --cflags libairspyhf)
CFLAGS_AIRSPY = -DHASAIRSPY $(shell pkg-config --cflags libairspy)
CFLAGS_SDRPLAY = -DHASSDRPLAY
CFLAGS_HACKRF = -DHASHACKRF $(shell pkg-config --cflags libhackrf) -I /usr/include/libhackrf/
CFLAGS_ZMQ = -DHASZMQ $(shell pkg-config --cflags libzmq)
CFLAGS_SOXR = -DHASSOXR $(shell pkg-config --cflags soxr)
CFLAGS_SAMPLERATE = -DHASSAMPLERATE $(shell pkg-config --cflags samplerate)
CFLAGS_CURL = -DHASCURL $(shell pkg-config --cflags libcurl)
CFLAGS_SSL = -DHASOPENSSL $(shell pkg-config --cflags openssl)
CFLAGS_SOAPYSDR = -DHASSOAPYSDR
CFLAGS_ZLIB = -DHASZLIB ${shell pkg-config --cflags zlib}
CFLAGS_PSQL  = -DHASPSQL ${shell pkg-config --cflags libpq}

LFLAGS_RTL = $(shell pkg-config --libs-only-l librtlsdr)
LFLAGS_AIRSPYHF = $(shell pkg-config --libs libairspyhf)
LFLAGS_AIRSPY = $(shell pkg-config --libs libairspy)
LFLAGS_SDRPLAY = -lsdrplay_api
LFLAGS_HACKRF = $(shell pkg-config --libs libhackrf)
LFLAGS_ZMQ = $(shell pkg-config --libs libzmq)
LFLAGS_SOXR = $(shell pkg-config --libs soxr)
LFLAGS_SAMPLERATE = $(shell pkg-config --libs samplerate)
LFLAGS_SOAPYSDR = -lSoapySDR
LFLAGS_CURL =$(shell pkg-config --libs libcurl)
LFLAGS_SSL =$(shell pkg-config --libs openssl)
LFLAGS_ZLIB =$(shell pkg-config --libs zlib)
LFLAGS_PSQL =$(shell pkg-config --libs libpq)


CFLAGS_ALL = -Wall -Wno-overloaded-virtual
LFLAGS_ALL =

ifneq ($(shell pkg-config --exists soxr && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SOXR)
    LFLAGS_ALL += $(LFLAGS_SOXR)
endif

ifneq ($(shell pkg-config --exists samplerate && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SAMPLERATE)
    LFLAGS_ALL += $(LFLAGS_SAMPLERATE)
endif

ifneq ($(shell pkg-config --exists librtlsdr && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_RTL)
    LFLAGS_ALL += $(LFLAGS_RTL)
endif

ifneq ($(shell pkg-config --exists libhackrf && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_HACKRF)
    LFLAGS_ALL += $(LFLAGS_HACKRF)
endif

ifneq ($(shell pkg-config --exists libairspy && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_AIRSPY)
    LFLAGS_ALL += $(LFLAGS_AIRSPY)
endif

ifneq ($(shell pkg-config --exists libairspyhf && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_AIRSPYHF)
    LFLAGS_ALL += $(LFLAGS_AIRSPYHF)
endif

ifneq ($(shell pkg-config --exists libzmq && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_ZMQ)
    LFLAGS_ALL += $(LFLAGS_ZMQ)
endif

ifneq ($(shell pkg-config --exists libcurl && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_CURL)
    LFLAGS_ALL += $(LFLAGS_CURL)
endif

ifneq ($(shell pkg-config --exists zlib && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_ZLIB)
    LFLAGS_ALL += $(LFLAGS_ZLIB)
endif

ifneq ($(shell pkg-config --exists libpq && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_PSQL)
    LFLAGS_ALL += $(LFLAGS_PSQL)
endif

ifneq ($(shell pkg-config --exists openssl && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SSL)
    LFLAGS_ALL += $(LFLAGS_SSL)
endif

# Building AIS-Catcher

all: lib
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_ALL)

rtl-only: lib-rtl
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_RTL)

airspyhf-only: lib-airspyhf
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_AIRSPYHF)

airspy-only: lib-airspy
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_AIRSPY)

sdrplay-only: lib-sdrplay
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_SDRPLAY)

hackrf-only: lib-hackrf
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_HACKRF)

zmq-only: lib-zmq
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_ZMQ)

soapysdr-only: lib-soapysdr
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_SOAPYSDR)

# Creating object-files
lib:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_ALL)

lib-rtl:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_RTL)

lib-airspyhf:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPYHF)

lib-airspy:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPY)

lib-sdrplay:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_SDRPLAY)

lib-hackrf:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_HACKRF)

lib-zmq:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_ZMQ)

lib-soapysdr:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_SOAPYSDR)

clean:
	rm *.o
	rm AIS-catcher

install:
	cp AIS-catcher /usr/local/bin/AIS-catcher
