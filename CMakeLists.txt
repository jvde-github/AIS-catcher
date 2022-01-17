# CMake definitions for AIS-catcher

cmake_minimum_required(VERSION 3.0)
project(AIS-catcher)

# Common compiler flags and options.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(OPTIMIZATION_FLAGS "-Ofast")
	add_compile_options("-Wno-psabi")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "CLANG")
	set(OPTIMIZATION_FLAGS "-Ofast")
endif()

if(MSVC)
	set(OPTIMIZATION_FLAGS "-DWIN32_LEAN_AND_MEAN  /EHsc ")
endif()

set(CMAKE_CXX_FLAGS "${OPTIMIZATION_FLAGS}")

# Use pkg-config
include(FindPkgConfig)
find_package(Threads)

# Define the libraries for inclusion
if(NOT MSVC)
	set(ADDITIONAL_LIBRARIES stdc++ m)
else()
	set(ADDITIONAL_LIBRARIES ws2_32.lib)
endif()

# Find libusb
pkg_check_modules(PKG_LIBUSB libusb-1.0)
find_path(LIBUSB_INCLUDE_DIR libusb.h HINT ${PKG_LIBUSB_INCLUDE_DIRS} PATH_SUFFIXES libusb-1.0)
find_library(LIBUSB_LIBRARY usb-1.0 HINT ${PKG_LIBUSB_LIBRARY_DIRS})

# Find RTL-SDR library.
pkg_check_modules(PKG_RTLSDR librtlsdr)
find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h HINT ${PKG_RTLSDR_INCLUDE_DIRS})
find_library(RTLSDR_LIBRARY rtlsdr HINT ${PKG_RTLSDR_LIBRARY_DIRS})

if(RTLSDR_INCLUDE_DIR AND RTLSDR_LIBRARY)
	message(STATUS "Found librtlsdr: ${RTLSDR_INCLUDE_DIR}, ${RTLSDR_LIBRARY}")
	add_definitions(-DHASRTLSDR)
	if(NOT MSVC)
		set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
		set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY} ${LIBUSB_LIBRARY})
	else()
		set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR})
		set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY})
	endif()
	if(PKG_RTLSDR_VERSION LESS "0.6")
		Message("RTLSDR: no support for biastee")
		add_definitions(-DLIBRTLSDR_LEGACY)
	endif()
else()
	message(WARNING "No RTLSDR support included.")
endif()

# Find SDRplay library.
find_path(SDRPLAY_INCLUDE_DIR sdrplay_api.h)
find_library(SDRPLAY_LIBRARY sdrplay_api)
	message(STATUS "Found sdrplay: ${SDRPLAY_INCLUDE_DIR}, ${SDRPLAY_LIBRARY}")

if(SDRPLAY_INCLUDE_DIR AND SDRPLAY_LIBRARY)
	message(STATUS "Found sdrplay: ${SDRPLAY_INCLUDE_DIR}, ${SDRPLAY_LIBRARY}")
	add_definitions(-DHASSDRPLAY)
	set(SDRPLAY_INCLUDE_DIRS ${SDRPLAY_INCLUDE_DIR})
	set(SDRPLAY_LIBRARIES ${SDRPLAY_LIBRARY})
else()
	message(WARNING "No SDRPLAY support included.")
endif()

# Find Airspy library.
pkg_check_modules(PKG_AIRSPY libairspy)
find_path(AIRSPY_INCLUDE_DIR airspy.h HINT ${PKG_AIRSPY_INCLUDE_DIRS})
find_library(AIRSPY_LIBRARY airspy HINT ${PKG_AIRSPY_LIBRARY_DIRS})

if(AIRSPY_INCLUDE_DIR AND AIRSPY_LIBRARY)
	message(STATUS "Found airspy: ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}")
	add_definitions(-DHASAIRSPY)
	set(AIRSPY_INCLUDE_DIRS ${AIRSPY_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
	set(AIRSPY_LIBRARIES ${AIRSPY_LIBRARY} ${LIBUSB_LIBRARY})
else()
	message(WARNING "No Airspy support included.")
endif()

# Find Airspy HF library.
pkg_check_modules(PKG_AIRSPYHF libairspy)
find_path(AIRSPYHF_INCLUDE_DIR airspyhf.h HINT ${PKG_AIRSPY_INCLUDE_DIRS})
find_library(AIRSPYHF_LIBRARY airspyhf HINT ${PKG_AIRSPY_LIBRARY_DIRS})

if(AIRSPYHF_INCLUDE_DIR AND AIRSPYHF_LIBRARY)
	message(STATUS "Found libairspyhf: ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}")
	add_definitions(-DHASAIRSPYHF)
	set(AIRSPYHF_INCLUDE_DIRS ${AIRSPYHF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
	set(AIRSPYHF_LIBRARIES ${AIRSPYHF_LIBRARY} ${LIBUSB_LIBRARY})
else()
	message(WARNING "No Airspy HF+ support included.")
endif()

# Find HackRF library.
pkg_check_modules(PKG_HACKRF libhackrf)
find_path(HACKRF_INCLUDE_DIR hackrf.h HINT ${PKG_HACKRF_INCLUDE_DIRS})
find_library(HACKRF_LIBRARY hackrf HINT ${PKG_HACKRF_LIBRARY_DIRS})


if(HACKRF_INCLUDE_DIR AND HACKRF_LIBRARY)
	message(STATUS "Found libhackrf: ${HACKRF_INCLUDE_DIR}, ${HACKRF_LIBRARY}")
	add_definitions(-DHASHACKRF)
	set(HACKRF_INCLUDE_DIRS ${HACKRF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
	set(HACKRF_LIBRARIES ${HACKRF_LIBRARY} ${LIBUSB_LIBRARY})
else()
	message(WARNING "No HACKRF support included.")
endif()

# Find libzmq
pkg_check_modules(PKG_LIBZMQ libzmq)
find_path(LIBZMQ_INCLUDE_DIR zmq.h HINT ${PKG_LIBZMQ_INCLUDE_DIRS})
find_library(LIBZMQ_LIBRARY zmq HINT ${PKG_LIBZMQ_LIBRARY_DIRS})

if(LIBZMQ_INCLUDE_DIR AND LIBZMQ_LIBRARY)
	message(STATUS "Found libzmq: ${LIBZMQ_INCLUDE_DIR}, ${LIBZMQ_LIBRARY}")
	add_definitions(-DHASZMQ)
	set(ZMQ_INCLUDE_DIRS ${LIBZMQ_INCLUDE_DIR})
	set(ZMQ_LIBRARIES ${LIBZMQ_LIBRARY})
else()
	message(WARNING "No ZMQ support included.")
endif()

set(CPP
	Application/Main.cpp
	Device/AIRSPYHF.cpp Device/FileWAV.cpp Device/RTLSDR.cpp Device/SDRPLAY.cpp DSP/Demod.cpp DSP/Model.cpp Library/AIS.cpp
	Device/FileRAW.cpp Device/HACKRF.cpp Device/RTLTCP.cpp Device/ZMQ.cpp
	Library/Utilities.cpp
	Device/AIRSPY.cpp
	DSP/DSP.cpp
	IO/IO.cpp)

set(APP_INCLUDES . ./Library ./DSP ./Application ./IO)

# Executable

add_executable(AIS-catcher ${CPP} )

include_directories(
	${AIRSPYHF_INCLUDE_DIRS} ${AIRSPY_INCLUDE_DIRS} ${HACKRF_INCLUDE_DIRS} ${RTLSDR_INCLUDE_DIRS} ${LIBZMQ_INCLUDE_DIRS} ${SDRPLAY_INCLUDE_DIRS}
	${APP_INCLUDES})

target_link_libraries(AIS-catcher
	${AIRSPY_LIBRARIES} ${AIRSPYHF_LIBRARIES} ${RTLSDR_LIBRARIES} ${HACKRF_LIBRARIES} ${ZMQ_LIBRARIES} ${SDRPLAY_LIBRARIES}
	${ADDITIONAL_LIBRARIES} Threads::Threads)

# Installation
install(TARGETS AIS-catcher DESTINATION bin)

# End of CMakeLists.txt
