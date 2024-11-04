#!/bin/bash

DEBUG_BUILD=false

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -d|--debug)
        DEBUG_BUILD=true
        shift
        ;;
        *)
        shift
        ;;
    esac
done

if [ -d "build" ]; then
  rm -rf build
  mkdir build
  echo "Build directory deleted and recreated."
else
  mkdir build
  echo "Build directory created"
fi

if [ -d "NMEA2000" ]; then
  rm -rf NMEA2000
  echo "NMEA2000 library directory deleted."
fi

# Attempt to clone the repository
git clone https://github.com/jvde-github/NMEA2000.git --depth 1

# Check if the clone was successful
if [ $? -ne 0 ]; then
  echo "Failed to clone NMEA2000 repository. Exiting script."
  exit 1
fi

# Proceed with the build process
cd NMEA2000/src
g++ -O3 -c  N2kMsg.cpp  N2kStream.cpp N2kMessages.cpp N2kTimer.cpp  NMEA2000.cpp  N2kGroupFunctionDefaultHandlers.cpp  N2kGroupFunction.cpp  -I.
ar rcs libnmea2000.a *.o 

cd ../../build
if [ "$DEBUG_BUILD" = true ]; then
    echo "Configuring AIS-catcher build in debug mode..."
    cmake .. -DNMEA2000_PATH=.. -DENABLE_DEBUG=ON
else
    echo "Configuring AIS-catcher build in release mode..."
    cmake .. -DNMEA2000_PATH=..
fi

make
