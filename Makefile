SRC = Tracking/Ships.cpp Marine/N2K.cpp IO/N2KInterface.cpp Device/N2KsktCAN.cpp IO/N2KStream.cpp Application/Prometheus.cpp Application/Main.cpp Application/WebViewer.cpp Application/MapTiles.cpp IO/HTTPClient.cpp DBMS/PostgreSQL.cpp Tracking/DB.cpp Application/Config.cpp Application/DeviceManager.cpp Application/Receiver.cpp IO/HTTPServer.cpp DSP/DSP.cpp JSON/JSONAIS.cpp JSON/Parser.cpp JSON/StringBuilder.cpp JSON/Keys.cpp Marine/AIS.cpp IO/Network.cpp DSP/Model.cpp Marine/NMEA.cpp DSP/Demod.cpp Marine/Message.cpp Device/UDP.cpp Device/ZMQ.cpp Device/RTLSDR.cpp Device/AIRSPYHF.cpp Device/SoapySDR.cpp Device/AIRSPY.cpp Device/FileRAW.cpp Device/FileWAV.cpp Device/SDRPLAY.cpp Device/RTLTCP.cpp Device/HACKRF.cpp Device/Serial.cpp Library/TCP.cpp Device/SpyServer.cpp JSON/JSON.cpp IO/Protocol.cpp IO/MsgOut.cpp IO/Screen.cpp Library/Logger.cpp Aviation/Basestation.cpp  Application/WebDB.cpp Aviation/Beast.cpp Aviation/ADSB.cpp Utilities/Parse.cpp Utilities/Convert.cpp Utilities/Helper.cpp Utilities/Serialize.cpp Utilities/TemplateString.cpp Utilities/StreamHelpers.cpp
OBJ = Ships.o Main.o N2KStream.o N2K.o N2KInterface.o N2KsktCAN.o Prometheus.o DeviceManager.o Receiver.o Config.o WebViewer.o MapTiles.o HTTPClient.o PostgreSQL.o DB.o DSP.o AIS.o Model.o Network.o Demod.o Serial.o RTLSDR.o HTTPServer.o AIRSPYHF.o Keys.o AIRSPY.o Parser.o StringBuilder.o FileRAW.o FileWAV.o SDRPLAY.o NMEA.o RTLTCP.o HACKRF.o ZMQ.o UDP.o SoapySDR.o TCP.o Message.o SpyServer.o JSON.o JSONAIS.o Protocol.o MsgOut.o Screen.o Logger.o Basestation.o WebDB.o Beast.o ADSB.o UtilParse.o UtilConvert.o UtilHelper.o UtilSerialize.o UtilTemplateString.o UtilStreamHelpers.o
INCLUDE = -I. -ISource -ISource/JSON/ -ISource/DBMS/ -ISource/Tracking/ -ISource/Library/ -ISource/Marine/ -ISource/Aviation/ -ISource/DSP/ -ISource/Application/ -ISource/IO/ -ISource/Utilities/ 
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

all: AIS-catcher

AIS-catcher: $(OBJ)
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_ALL)

rtl-only: CFLAGS_ALL = $(CFLAGS_RTL)
rtl-only: LFLAGS_ALL = $(LFLAGS_RTL)
rtl-only: AIS-catcher

airspyhf-only: CFLAGS_ALL = $(CFLAGS_AIRSPYHF)
airspyhf-only: LFLAGS_ALL = $(LFLAGS_AIRSPYHF)
airspyhf-only: AIS-catcher

airspy-only: CFLAGS_ALL = $(CFLAGS_AIRSPY)
airspy-only: LFLAGS_ALL = $(LFLAGS_AIRSPY)
airspy-only: AIS-catcher

sdrplay-only: CFLAGS_ALL = $(CFLAGS_SDRPLAY)
sdrplay-only: LFLAGS_ALL = $(LFLAGS_SDRPLAY)
sdrplay-only: AIS-catcher

hackrf-only: CFLAGS_ALL = $(CFLAGS_HACKRF)
hackrf-only: LFLAGS_ALL = $(LFLAGS_HACKRF)
hackrf-only: AIS-catcher

zmq-only: CFLAGS_ALL = $(CFLAGS_ZMQ)
zmq-only: LFLAGS_ALL = $(LFLAGS_ZMQ)
zmq-only: AIS-catcher

soapysdr-only: CFLAGS_ALL = $(CFLAGS_SOAPYSDR)
soapysdr-only: LFLAGS_ALL = $(LFLAGS_SOAPYSDR)
soapysdr-only: AIS-catcher

# Pattern rule for compiling .cpp files to .o files
%.o: Source/Application/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Tracking/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/DBMS/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Device/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/DSP/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/IO/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/JSON/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Library/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Marine/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Aviation/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

# Special rules for Utilities directory to avoid name conflicts
UtilParse.o: Source/Utilities/Parse.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

UtilConvert.o: Source/Utilities/Convert.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

UtilHelper.o: Source/Utilities/Helper.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

UtilSerialize.o: Source/Utilities/Serialize.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

UtilTemplateString.o: Source/Utilities/TemplateString.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

UtilStreamHelpers.o: Source/Utilities/StreamHelpers.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

clean:
	rm *.o
	rm AIS-catcher

install:
	cp AIS-catcher /usr/local/bin/AIS-catcher
