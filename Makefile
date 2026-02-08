SRC = Application/CLI.cpp Application/ApplicationState.cpp Application/JSONConfig.cpp Application/DeviceManager.cpp Application/Main.cpp Application/Receiver.cpp WebViewer/WebViewer.cpp WebViewer/WebDB.cpp WebViewer/SSEStreamer.cpp WebViewer/Persistence.cpp WebViewer/Configuration.cpp WebViewer/RouteHandlers.cpp WebViewer/MapTiles.cpp WebViewer/Prometheus.cpp DBMS/PostgreSQL.cpp Device/AIRSPY.cpp Device/AIRSPYHF.cpp Device/FileRAW.cpp Device/FileWAV.cpp Device/HACKRF.cpp Device/HYDRASDR.cpp Device/N2KsktCAN.cpp Device/RTLSDR.cpp Device/RTLTCP.cpp Device/SDRPLAY.cpp Device/Serial.cpp Device/SoapySDR.cpp Device/SpyServer.cpp Device/UDP.cpp Device/ZMQ.cpp DSP/Demod.cpp DSP/DSP.cpp DSP/Model.cpp IO/HTTPClient.cpp IO/HTTPServer.cpp IO/MsgOut.cpp IO/Screen.cpp IO/N2KInterface.cpp IO/N2KStream.cpp IO/Network.cpp IO/CommunityStreamer.cpp IO/Protocol.cpp JSON/JSON.cpp JSON/JSONAIS.cpp JSON/JSONBuilder.cpp JSON/Keys.cpp JSON/Parser.cpp JSON/StringBuilder.cpp Aviation/ADSB.cpp Aviation/Basestation.cpp Aviation/Beast.cpp Marine/AIS.cpp Marine/Message.cpp Marine/N2K.cpp Marine/NMEA.cpp Library/Logger.cpp IO/TCPServer.cpp Tracking/DB.cpp Tracking/Ships.cpp Utilities/Parse.cpp Utilities/Convert.cpp Utilities/Helper.cpp Utilities/Serialize.cpp Utilities/TemplateString.cpp Utilities/StreamHelpers.cpp
OBJ = CLI.o ApplicationState.o JSONConfig.o DeviceManager.o Main.o Receiver.o WebViewer.o WebDB.o SSEStreamer.o Persistence.o Configuration.o RouteHandlers.o MapTiles.o Prometheus.o PostgreSQL.o AIRSPY.o AIRSPYHF.o FileRAW.o FileWAV.o HACKRF.o HYDRASDR.o N2KsktCAN.o RTLSDR.o RTLTCP.o SDRPLAY.o Serial.o SoapySDR.o SpyServer.o UDP.o ZMQ.o Demod.o DSP.o Model.o HTTPClient.o HTTPServer.o MsgOut.o Screen.o N2KInterface.o N2KStream.o Network.o CommunityStreamer.o Protocol.o JSON.o JSONAIS.o JSONBuilder.o Keys.o Parser.o StringBuilder.o ADSB.o Basestation.o Beast.o AIS.o Message.o N2K.o NMEA.o Logger.o TCPServer.o DB.o Ships.o Parse.o Convert.o Helper.o Serialize.o TemplateString.o StreamHelpers.o
INCLUDE = -I. -ISource -ISource/JSON/ -ISource/DBMS/ -ISource/Tracking/ -ISource/Library/ -ISource/Marine/ -ISource/Aviation/ -ISource/DSP/ -ISource/Application/ -ISource/WebViewer/ -ISource/IO/ -ISource/Utilities/ 
CC = clang

override CFLAGS +=  -Ofast -std=c++11 -g -pg -Wno-sign-compare $(INCLUDE)
override LFLAGS += -lstdc++ -lpthread -g -pg -lm  -o AIS-catcher

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

%.o: Source/WebViewer/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

%.o: Source/Utilities/%.cpp
	$(CC) -c $< $(CFLAGS) $(CFLAGS_ALL) -o $@

clean:
	rm *.o
	rm AIS-catcher

install:
	cp AIS-catcher /usr/local/bin/AIS-catcher
