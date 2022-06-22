SRC = Application/Main.cpp IO/IO.cpp DSP/DSP.cpp Library/AIS.cpp DSP/Model.cpp Library/Utilities.cpp DSP/Demod.cpp Device/ZMQ.cpp Device/RTLSDR.cpp Device/AIRSPYHF.cpp Device/SoapySDR.cpp Device/AIRSPY.cpp Device/FileRAW.cpp Device/FileWAV.cpp Device/SDRPLAY.cpp Device/RTLTCP.cpp Device/HACKRF.cpp Library/TCP.cpp
OBJ = Main.o IO.o DSP.o AIS.o Model.o Utilities.o Demod.o RTLSDR.o AIRSPYHF.o AIRSPY.o FileRAW.o FileWAV.o SDRPLAY.o RTLTCP.o HACKRF.o ZMQ.o SoapySDR.o TCP.o
INCLUDE = -I. -ILibrary/ -IDSP/ -IApplication/ -IIO/
CC = gcc

override CFLAGS += -Ofast -std=c++11 $(INCLUDE)
override LFLAGS += -lstdc++ -lpthread -lm -o AIS-catcher

CFLAGS_RTL = -DHASRTLSDR $(shell pkg-config --cflags librtlsdr)
CFLAGS_AIRSPYHF = -DHASAIRSPYHF $(shell pkg-config --cflags libairspyhf)
CFLAGS_AIRSPY = -DHASAIRSPY $(shell pkg-config --cflags libairspy)
CFLAGS_SDRPLAY = -DHASSDRPLAY
CFLAGS_HACKRF = -DHASHACKRF $(shell pkg-config --cflags libhackrf) -I /usr/include/libhackrf/
CFLAGS_ZMQ = -DHASZMQ $(shell pkg-config --cflags libzmq)
CFLAGS_SOXR = -DHASSOXR $(shell pkg-config --cflags soxr)
CFLAGS_SOAPYSDR = -DHASSOAPYSDR

LFLAGS_RTL = $(shell pkg-config --libs-only-l librtlsdr)
LFLAGS_AIRSPYHF = $(shell pkg-config --libs libairspyhf)
LFLAGS_AIRSPY = $(shell pkg-config --libs libairspy)
LFLAGS_SDRPLAY = -lsdrplay_api
LFLAGS_HACKRF = $(shell pkg-config --libs libhackrf)
LFLAGS_ZMQ = $(shell pkg-config --libs libzmq)
LFLAGS_SOXR = $(shell pkg-config --libs soxr)
LFLAGS_SOAPYSDR = -lSoapySDR

CFLAGS_ALL =
LFLAGS_ALL =

ifneq ($(shell pkg-config --exists soxr && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SOXR)
    LFLAGS_ALL += $(LFLAGS_SOXR)
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
