SRC = Main.cpp IO.cpp DSP.cpp Device.cpp AIS.cpp Model.cpp Utilities.cpp
OBJ = Main.o IO.o DSP.o Device.o AIS.o Model.o Utilities.o

CC = gcc 
CFLAGS = -O3  
LFLAGS = -lstdc++ -lm -o AIS-catcher 

CFLAGS_RTL = -DHASRTLSDR 
CFLAGS_AIRSPYHF = -DHASAIRSPYHF 

LFLAGS_RTL = -lrtlsdr -lpthread
LFLAGS_AIRSPYHF = -lairspyhf -lpthread

all: lib
	$(CC) $(OBJ) $(LFLAGS_AIRSPYHF) $(LFLAGS_RTL) $(LFLAGS)

rtl-only: lib-rtl
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_RTL)

airspyhf-only: lib-airspyhf
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_AIRSPYHF)

lib: 
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPYHF) $(CFLAGS_RTL)

lib-rtl:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_RTL)

lib-airspyhf:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_AIRSPYHF)

clean:
	rm *.o 
	rm AIS-catcher
