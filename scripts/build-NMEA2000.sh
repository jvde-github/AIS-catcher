if [ -d "build" ]; then
  rm -rf build
  mkdir build
  echo "Build directory deleted and recreated."
else
  mkdir build
  echo "build directory created"
fi

if [ -d "NMEA2000" ]; then
  rm -rf NMEA2000
  echo "NMEA2000 library directory deleted."
fi

wget https://github.com/ttlappalainen/NMEA2000/archive/refs/heads/master.zip
unzip master.zip
mv NMEA2000-master NMEA2000
cd NMEA2000; cd src
g++ -O3 -c  N2kMsg.cpp  N2kStream.cpp N2kMessages.cpp   N2kTimer.cpp  NMEA2000.cpp  N2kGroupFunctionDefaultHandlers.cpp  N2kGroupFunction.cpp  -I.
ar rcs libnmea2000.a *.o 
cd ../../build; ls ..; cmake .. -DNMEA2000_PATH=..; make
