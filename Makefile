SRC = Main.cpp Output/IO.cpp DSP.cpp AIS.cpp Model.cpp Utilities.cpp Demod.cpp Input/DeviceRTLSDR.cpp Input/DeviceAIRSPYHF.cpp Input/DeviceAIRSPY.cpp Input/DeviceFileRAW.cpp Input/DeviceFileWAV.cpp Input/DeviceSDRPLAY.cpp Input/DeviceRTLTCP.cpp Input/DeviceHACKRF.cpp
OBJ = Main.o IO.o DSP.o AIS.o Model.o Utilities.o Demod.o DeviceRTLSDR.o DeviceAIRSPYHF.o DeviceAIRSPY.o DeviceFileRAW.o DeviceFileWAV.o DeviceSDRPLAY.o DeviceRTLTCP.o DeviceHACKRF.o

CC = gcc
override CFLAGS += -Ofast -std=c++11
override LFLAGS += -lstdc++ -lm -o AIS-catcher

CFLAGS_RTL = -DHASRTLSDR
CFLAGS_AIRSPYHF = -DHASAIRSPYHF
CFLAGS_AIRSPY = -DHASAIRSPY
CFLAGS_SDRPLAY = -DHASSDRPLAY
CFLAGS_RTLTCP = -DHASRTLTCP
CFLAGS_HACKRF = -DHASHACKRF

LFLAGS_RTL = -lrtlsdr -lpthread
LFLAGS_AIRSPYHF = -lairspyhf -lpthread
LFLAGS_AIRSPY = -lairspy -lpthread
LFLAGS_SDRPLAY = -lsdrplay_api -lpthread
LFLAGS_RTLTCP = -lpthread
LFLAGS_HACKRF = -lpthread -lhackrf

all: lib
	$(CC) $(OBJ) $(LFLAGS_AIRSPYHF) $(LFLAGS_AIRSPY) $(LFLAGS_RTL) $(LFLAGS) $(LFLAGS_RTLTCP) $(LFLAGS_HACKRF)

rtl-only: lib-rtl
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_RTL)

airspyhf-only: lib-airspyhf
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_AIRSPYHF)

airspy-only: lib-airspy
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_AIRSPY)

sdrplay-only: lib-sdrplay
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_SDRPLAY)

rtltcp-only: lib-rtltcp
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_RTLTCP)

hackrf-only: lib-hackrf
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_HACKRF)

lib: 
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPYHF) $(CFLAGS_AIRSPY) $(CFLAGS_RTL) $(CFLAGS_RTLTCP) $(CFLAGS_HACKRF)

lib-rtl:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_RTL)

lib-airspyhf:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPYHF)

lib-airspy:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPY)

lib-sdrplay:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_SDRPLAY)

lib-rtltcp:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_RTLTCP)

lib-hackrf:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_HACKRF)

clean:
	rm *.o 
	rm AIS-catcher

install:
	cp AIS-catcher /usr/local/bin/AIS-catcher
