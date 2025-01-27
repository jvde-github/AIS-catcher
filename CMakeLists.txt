# CMake definitions for AIS-catcher

cmake_minimum_required(VERSION 3.02)
project(AIS-catcher)

option(SOAPYSDR "Include Soapy SDR support" OFF)
option(RTLSDR "Include RTLSDR support" ON)
option(RTLSDR_STATIC "Statically link RTLSDR library" OFF)
option(AIRSPY "Include Airspy support" ON)
option(SDRPLAY "Include SDRPLAY support" ON)
option(AIRSPYHF "Include Airspy HF+ support" ON)
option(HACKRF "Include HackRF support" ON)
option(SOXR "Include SOXR support" ON)
option(CURL "Include CURL support" OFF)
option(ZLIB "Include ZLIB support" ON)
option(SAMPLERATE "Include SAMPLERATE support" ON)
option(ZMQ "Include ZMQ support" ON)
option(PSQL "Include PSQL support" ON)
option(SQLITE "Include SQLITE support" ON)
option(OPENSSL "Include OPENSSL support" ON)
option(NMEA2000 "Include NMEA2000 support" ON)
option(ARMV6 "Compile for Raspberry Pi Zero" OFF)

set(NMEA2000_PATH "." CACHE PATH "Path to NMEA2000 library")

# Common compiler flags and options.

option(MSVC_VCPKG "For MSVC use VCPKG libraries." OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(OPTIMIZATION_FLAGS "-Ofast")
    add_compile_options("-Wall")
    add_compile_options("-Wno-psabi")
    add_compile_options("-Wno-sign-compare")
    set(OPTIMIZATION_FLAGS "-Ofast")
elseif(MSVC)
    set(OPTIMIZATION_FLAGS "-DWIN32_LEAN_AND_MEAN /EHsc /Oi /Ot /fp:fast")
endif()

set(CMAKE_CXX_FLAGS "${OPTIMIZATION_FLAGS}")

include(FindPkgConfig)
include(CheckIncludeFile)
include(CheckSymbolExists)

# Use pkg-config
find_package(Threads)

if(MSVC)
    add_definitions(-DHASEVENTLOG)
elseif(APPLE)
    check_include_file("os/log.h" HASOSLOG)
    if(HASOSLOG)
        add_definitions(-DHASOSLOG)
    endif()
elseif(UNIX)
    check_include_file("syslog.h" HASSYSLOG)
    if(HASSYSLOG)
        
        check_symbol_exists(syslog "syslog.h" CAN_USE_SYSLOG)
        if(CAN_USE_SYSLOG)
            add_definitions(-DHASSYSLOG)
        endif()
    endif()
endif()

# Debug symbols and optimization settings for AIS-catcher
if(UNIX AND NOT APPLE)
    find_library(DL_LIBRARY dl REQUIRED)
    if(ENABLE_DEBUG)
        add_compile_options(
            -g3
            -ggdb
            -O0
            -fno-omit-frame-pointer
            -rdynamic
            -no-pie
        )
        add_definitions(-DDEBUG)
        message(STATUS "Debug symbols enabled for Linux build via ENABLE_DEBUG")
    endif()
endif()

# for MSVC we link to the PothosSDR install
if(MSVC AND NOT MSVC_VCPKG)
    if(NOT POTHOSSDR_BINARY_DIR)
        SET(POTHOSSDR_BINARY_DIR $ENV{PROGRAMFILES}\\PothosSDR\\bin)
    endif()
    if(NOT POTHOSSDR_LIBRARY_DIR)
        SET(POTHOSSDR_LIBRARY_DIR $ENV{PROGRAMFILES}\\PothosSDR\\lib)
    endif()
    if(NOT POTHOSSDR_INCLUDE_DIR)
        SET(POTHOSSDR_INCLUDE_DIR $ENV{PROGRAMFILES}\\PothosSDR\\include)
    endif()
endif()

# Define the libraries for inclusion
if(MSVC)
    set(ADDITIONAL_LIBRARIES ws2_32.lib)
endif()

# Find NMEA2000 support on Linux

if(NOT MSVC AND NMEA2000)


    check_include_file("linux/can.h" HAVE_CAN_HEADER)
    check_include_file("linux/can/raw.h" HAVE_RAW_CAN_HEADER)

    if(HAVE_CAN_HEADER AND HAVE_RAW_CAN_HEADER)
        find_file(NMEA2000_LIB NAMES libnmea2000.a PATHS ${NMEA2000_PATH} ${NMEA2000_PATH}/src ${NMEA2000_PATH}/NMEA2000/src)
        find_path(NMEA2000_INCLUDE NAMES NMEA2000.h PATHS PATHS ${NMEA2000_PATH} ${NMEA2000_PATH}/src ${NMEA2000_PATH}/NMEA2000/src)

        if(NMEA2000_LIB AND NMEA2000_INCLUDE)
            add_definitions(-DHASNMEA2000)
            set(NMEA2000_INCLUDE_DIRS ${NMEA2000_INCLUDE})
            set(NMEA2000_LIBRARIES ${NMEA2000_LIB})
            message(STATUS "NMEA2000: found ${NMEA2000_LIBRARIES} ${NMEA2000_INCLUDE_DIRS}")
        else()
            message(STATUS "NMEA2000: not found  ${NMEA2000_LIB} ${NMEA2000_INCLUDE}")
        endif()
    else()
        message(STATUS "Required headers (can.h and raw/can.h) not found. ${HAVE_CAN_HEADER} ${HAVE_RAW_CAN_HEADER}")
    endif()
endif()

# Find libusb, pthread
if(NOT MSVC)
    pkg_check_modules(PKG_LIBUSB libusb-1.0)
    find_path(LIBUSB_INCLUDE_DIR libusb.h HINTS ${PKG_LIBUSB_INCLUDE_DIRS} PATH_SUFFIXES libusb-1.0)
    find_library(LIBUSB_LIBRARY usb-1.0 HINTS ${PKG_LIBUSB_LIBRARY_DIRS})
elseif(NOT MSVC_VCPKG)
    find_file(LIBUSB_DLL libusb-1.0.dll HINTS ${POTHOSSDR_BINARY_DIR})
    find_file(PTHREAD_DLL pthreadVC3.dll HINTS ${POTHOSSDR_BINARY_DIR})
else()
    set(LIBUSB_INCLUDE_DIR "")
    set(LIBUSB_LIBRARY "")
endif()

if(MSVC AND NOT MSVC_VCPKG)
    if(EXISTS "${POTHOSSDR_BINARY_DIR}/libssl-3-x64.dll")
        set(OPENSSL_DLL "${POTHOSSDR_BINARY_DIR}/libssl-3-x64.dll")
    elseif(EXISTS "${POTHOSSDR_BINARY_DIR}/libssl-3.dll")
        set(OPENSSL_DLL "${POTHOSSDR_BINARY_DIR}/libssl-3.dll")
    endif()
    message(STATUS "OpenSSL DLL: ${OPENSSL_DLL}")

    if(EXISTS "${POTHOSSDR_BINARY_DIR}/libcrypto-3-x64.dll")
        set(CRYPTO_DLL "${POTHOSSDR_BINARY_DIR}/libcrypto-3-x64.dll")
    elseif(EXISTS "${POTHOSSDR_BINARY_DIR}/libcrypto-3.dll")
        set(CRYPTO_DLL "${POTHOSSDR_BINARY_DIR}/libcrypto-3.dll")
    endif()
    message(STATUS "Crypto DLL: ${CRYPTO_DLL}")
endif()

# Find ssl
if(OPENSSL)
    if(MSVC AND NOT MSVC_VCPKG)

        find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(OPENSSL_LIBRARY libssl.lib HINTS ${POTHOSSDR_LIBRARY_DIR})

        if(OPENSSL_INCLUDE_DIR AND OPENSSL_LIBRARY AND OPENSSL_DLL AND CRYPTO_DLL)
            Message(STATUS "OPENSSL: found (PothosSDR) - " ${OPENSSL_INCLUDE_DIR}, ${OPENSSL_LIBRARY}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
            set(COPY_OPENSSL_DLL TRUE)
            add_definitions(-DHASOPENSSL)

            set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
            set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARY})
        else()
            Message(STATUS "OPENSSL: NOT found (PothosSDR) - " ${OPENSSL_INCLUDE_DIR}, ${OPENSSL_LIBRARY}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
        endif()
    else()

        pkg_check_modules(PKG_SSL libssl)
        find_path(SSL_INCLUDE_DIR openssl/ssl.h HINTS ${PKG_SSL_INCLUDE_DIRS})
        find_library(SSL_LIBRARY NAMES libssl.so libssl.dylib HINTS ${PKG_SSL_LIBRARY_DIRS})
        find_library(CRYPTO_LIBRARY NAMES libcrypto.so libcrypto.dylib HINTS ${PKG_SSL_LIBRARY_DIRS})

        if(SSL_INCLUDE_DIR AND SSL_LIBRARY AND CRYPTO_LIBRARY)
            message(STATUS "OPENSSL: found - ${SSL_INCLUDE_DIR}, ${SSL_LIBRARY}, ${CRYPTO_LIBRARY}")
            add_definitions(-DHASOPENSSL)
            set(OPENSSL_INCLUDE_DIRS ${SSL_INCLUDE_DIR})
            set(OPENSSL_LIBRARIES ${SSL_LIBRARY} ${CRYPTO_LIBRARY})
        else()
            message(STATUS "OPENSSL: not found - ${SSL_INCLUDE_DIR}, ${SSL_LIBRARY}, ${CRYPTO_LIBRARY}")
        endif()

    endif()
endif()


# Find libcurl
if(CURL)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(CURL_INCLUDE_DIR curl/curl.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(CURL_LIBRARY libcurl.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(CURL_DLL libcurl.dll HINTS ${POTHOSSDR_BINARY_DIR})
        find_file(ZLIB1_DLL zlib1.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(CURL_INCLUDE_DIR AND CURL_LIBRARY AND CURL_DLL AND ZLIB1_DLL)
            Message(STATUS "CURL: found (PothosSDR) - " ${CURL_INCLUDE_DIR}, ${CURL_LIBRARY}, ${CURL_DLL}, ${ZLIB1_DLL})
            set(COPY_CURL_DLL TRUE)
            set(COPY_ZLIB1_DLL TRUE)
            add_definitions(-DHASCURL)

            set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
            set(CURL_LIBRARIES ${CURL_LIBRARY})
        else()
            Message(STATUS "CURL: NOT found (PothosSDR) - " ${CURL_INCLUDE_DIR}, ${CURL_LIBRARY}, ${CURL_DLL})
        endif()
    else()
        pkg_check_modules(PKG_CURL libcurl)
        find_path(CURL_INCLUDE_DIR curl/curl.h HINTS ${PKG_CURL_INCLUDE_DIRS})
        find_library(CURL_LIBRARY libcurl.so HINTS ${PKG_CURL_LIBRARY_DIRS})
        find_library(CURL_LIBRARY libcurl.dylib HINTS ${PKG_CURL_LIBRARY_DIRS})
        find_library(CURL_LIBRARY curl HINTS ${PKG_CURL_LIBRARY_DIRS})

        if(CURL_INCLUDE_DIR AND CURL_LIBRARY)

            message(STATUS "CURL: found - ${CURL_INCLUDE_DIR}, ${CURL_LIBRARY}")
            add_definitions(-DHASCURL)

            set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
            set(CURL_LIBRARIES ${CURL_LIBRARY})

        else()
            message(STATUS "CURL: not found - ${CURL_INCLUDE_DIR}, ${CURL_LIBRARY}")
        endif()
    endif()
endif()

# Find zlib
if(ZLIB)
    if(MSVC)
        find_path(ZLIB_INCLUDE_DIR zlib.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(ZLIB_LIBRARY zlib.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(ZLIB_DLL zlib1.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY AND ZLIB_DLL)
            Message(STATUS "ZLIB: found (PothosSDR) - " ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}, ${ZLIB_DLL})
            set(COPY_ZLIB_DLL TRUE)
            add_definitions(-DHASZLIB)

            set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
            set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
        else()
            Message(STATUS "ZLIB: not found (PothosSDR) - " ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}, ${ZLIB_DLL})
        endif()

        add_definitions(-DHASZLIB)

    else()
        pkg_check_modules(PKG_ZLIB zlib)
        find_path(ZLIB_INCLUDE_DIR zlib.h HINTS ${PKG_ZLIB_INCLUDE_DIRS})
        find_library(ZLIB_LIBRARY libz.so HINTS ${PKG_ZLIB_LIBRARY_DIRS})
        find_library(ZLIB_LIBRARY libz.dylib HINTS ${PKG_ZLIB_LIBRARY_DIRS})
        find_library(ZLIB_LIBRARY libz.tbd HINTS ${PKG_ZLIB_LIBRARY_DIRS})
        find_library(ZLIB_LIBRARY libz.dll HINTS ${PKG_ZLIB_LIBRARY_DIRS})

        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)

            message(STATUS "ZLIB: found - ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}")
            add_definitions(-DHASZLIB)

            set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
            set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})

        else()
            message(STATUS "ZLIB: not found - ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}")
        endif()
    endif()
endif()


# Find libpq
if(PSQL)

    if(MSVC AND NOT MSVC_VCPKG)
        find_path(PQ_INCLUDE_DIR libpq-fe.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(PQ_LIBRARY libpq.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(PQ_DLL libpq.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(PQ_INCLUDE_DIR AND PQ_LIBRARY AND PQ_DLL AND OPENSSL_DLL AND CRYPTO_DLL)
            Message(STATUS "LIBPQ: found (PothosSDR) - " ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}, ${PQ_DLL}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
            set(COPY_OPENSSL_DLL TRUE)
            set(COPY_PQ_DLL TRUE)
            add_definitions(-DHASPSQL)

            set(PQ_INCLUDE_DIRS ${PQ_INCLUDE_DIR})
            set(PQ_LIBRARIES ${PQ_LIBRARY})

        else()
            Message(STATUS "LIBPQ: Not found (PothosSDR) - " ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}, ${PQ_DLL}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
        endif()
    else()
        pkg_check_modules(PKG_PQ libpq)
        find_path(PQ_INCLUDE_DIR libpq-fe.h HINTS ${PKG_PQ_INCLUDE_DIRS})
        find_library(PQ_LIBRARY libpq.so HINTS ${PKG_PQ_LIBRARY_DIRS})
        find_library(PQ_LIBRARY libpq.dylib HINTS ${PKG_PQ_LIBRARY_DIRS})
        find_library(PQ_LIBRARY pq HINTS ${PKG_PQ_LIBRARY_DIRS})

        if(PQ_INCLUDE_DIR AND PQ_LIBRARY)

            message(STATUS "PQ: found - ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}")

            set(PQ_INCLUDE_DIRS ${PQ_INCLUDE_DIR})
            set(PQ_LIBRARIES ${PQ_LIBRARY})
            add_definitions(-DHASPSQL)
        else()
            message(STATUS "PQ: not found - ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}")
        endif()
    endif()
endif()

# Find sqlite
if(SQLITE)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SQLITE_INCLUDE_DIR sqlite3.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(SQLITE_LIBRARY sqlite3.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(SQLITE_DLL sqlite3.dll HINTS ${POTHOSSDR_BINARY_DIR})
        
        if(SQLITE_INCLUDE_DIR AND SQLITE_LIBRARY AND SQLITE_DLL)
        message(STATUS "SQLite: found (PothosSDR) - " ${SQLITE_INCLUDE_DIR}, ${SQLITE_LIBRARY}, ${SQLITE_DLL})
            set(COPY_SQLITE_DLL TRUE)
            add_definitions(-DHASSQLITE)
            set(SQLITE_INCLUDE_DIRS ${SQLITE_INCLUDE_DIR})
            set(SQLITE_LIBRARIES ${SQLITE_LIBRARY})
        else()
            message(STATUS "SQLite: Not found (PothosSDR) - " ${SQLITE_INCLUDE_DIR}, ${SQLITE_LIBRARY}, ${SQLITE_DLL})
        endif()
    else()
        # For Unix-like systems, use pkg-config if available
        pkg_check_modules(PKG_SQLITE sqlite3)
        
        find_path(SQLITE_INCLUDE_DIR sqlite3.h HINTS ${PKG_SQLITE_INCLUDE_DIRS})
        find_library(SQLITE_LIBRARY sqlite3 HINTS ${PKG_SQLITE_LIBRARY_DIRS})
        
        if(SQLITE_INCLUDE_DIR AND SQLITE_LIBRARY)
            message(STATUS "SQLite: found - ${SQLITE_INCLUDE_DIR}, ${SQLITE_LIBRARY}")
            set(SQLITE_INCLUDE_DIRS ${SQLITE_INCLUDE_DIR})
            set(SQLITE_LIBRARIES ${SQLITE_LIBRARY})
            add_definitions(-DHASSQLITE)
        else()
            message(STATUS "SQLite: not found - ${SQLITE_INCLUDE_DIR}, ${SQLITE_LIBRARY}")
        endif()
    endif()
endif()


# Find libsoxr
if(SOXR)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SOXR_INCLUDE_DIR soxr.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(SOXR_LIBRARY soxr.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(SOXR_DLL soxr.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(SOXR_INCLUDE_DIR AND SOXR_LIBRARY AND SOXR_DLL)
            Message(STATUS "SOXR: found (PothosSDR) - " ${SOXR_INCLUDE_DIR}, ${SOXR_LIBRARY}, ${SOXR_DLL})
            set(COPY_SOXR_DLL TRUE)
            add_definitions(-DHASSOXR)

            set(SOXR_INCLUDE_DIRS ${SOXR_INCLUDE_DIR})
            set(SOXR_LIBRARIES ${SOXR_LIBRARY})

        else()
            Message(STATUS "SOXR: NOT found (PothosSDR) - " ${SOXR_INCLUDE_DIR}, ${SOXR_LIBRARY}, ${SOXR_DLL})
        endif()
    else()
        pkg_check_modules(PKG_SOXR soxr)
        find_path(SOXR_INCLUDE_DIR soxr.h HINTS ${PKG_SOXR_INCLUDE_DIRS})
        find_library(SOXR_LIBRARY libsoxr.so HINTS ${PKG_SOXR_LIBRARY_DIRS})
        find_library(SOXR_LIBRARY libsoxr.dylib HINTS ${PKG_SOXR_LIBRARY_DIRS})
        find_library(SOXR_LIBRARY soxr HINTS ${PKG_SOXR_LIBRARY_DIRS})

        if(SOXR_INCLUDE_DIR AND SOXR_LIBRARY)

            message(STATUS "SOXR: found - ${SOXR_INCLUDE_DIR}, ${SOXR_LIBRARY}")
            add_definitions(-DHASSOXR)

            set(SOXR_INCLUDE_DIRS ${SOXR_INCLUDE_DIR})
            set(SOXR_LIBRARIES ${SOXR_LIBRARY})

        else()
            Message(STATUS "SOXR: not found.")
        endif()
    endif()
endif()

# Find libsoxr
if(SAMPLERATE)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SAMPLERATE_INCLUDE_DIR samplerate.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(SAMPLERATE_LIBRARY samplerate.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(SAMPLERATE_DLL samplerate.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(SAMPLERATE_INCLUDE_DIR AND SAMPLERATE_LIBRARY AND SOXR_DLL)
            Message(STATUS "SAMPLERATE: found (PothosSDR) - " ${SAMPLERATE_INCLUDE_DIR}, ${SAMPLERATE_LIBRARY}, ${SAMPLERATE_DLL})
            set(COPY_SAMPLERATE_DLL TRUE)
            add_definitions(-DHASSAMPLERATE)

            set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE_INCLUDE_DIR})
            set(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIBRARY})

        else()
            Message(STATUS "libsamplerate: NOT found (PothosSDR) - " ${SAMPLERATE_INCLUDE_DIR}, ${SAMPLERATE_LIBRARY}, ${SAMPLERATE_DLL})
        endif()
    else()
        pkg_check_modules(PKG_SOXR samplerate)
        find_path(SAMPLERATE_INCLUDE_DIR samplerate.h HINTS ${PKG_SAMPLERATE_INCLUDE_DIRS})
        find_library(SAMPLERATE_LIBRARY libsamplerate.so HINTS ${PKG_SAMPLERATE_LIBRARY_DIRS})
        find_library(SAMPLERATE_LIBRARY libsamplerate.dylib HINTS ${PKG_SAMPLERATE_LIBRARY_DIRS})
        find_library(SAMPLERATE_LIBRARY samplerate HINTS ${PKG_SAMPLERATE_LIBRARY_DIRS})

        if(SAMPLERATE_INCLUDE_DIR AND SAMPLERATE_LIBRARY)

            message(STATUS "libsamplerate: found - ${SAMPLERATE_INCLUDE_DIR}, ${SAMPLERATE_LIBRARY}")
            add_definitions(-DHASSAMPLERATE)

            set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE_INCLUDE_DIR})
            set(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIBRARY})

        else()
            Message(STATUS "libsamplerate: not found.")
        endif()
    endif()
endif()

if(SOAPYSDR)

    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SOAPYSDR_INCLUDE_DIR SOapySDR/Device.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(SOAPYSDR_LIBRARY SoapySDR.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(SOAPYSDR_DLL SoapySDR.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(SOAPYSDR_INCLUDE_DIR AND SOAPYSDR_LIBRARY AND SOAPYSDR_DLL)
            Message(STATUS "SOAPYSDR: found (PothosSDR) - " ${SOAPYSDR_INCLUDE_DIR}, ${SOAPYSDR_LIBRARY}, ${SOAPYSDR_DLL})
            set(COPYSOAPYSDR TRUE)
            add_definitions(-DHASSOAPYSDR)

            set(SOAPYSDR_INCLUDE_DIRS ${SOAPYSDR_INCLUDE_DIR})
            set(SOAPYSDR_LIBRARIES ${SOAPYSDR_LIBRARY})

        else()
            Message(STATUS "SOAPYSDR: NOT found (PothosSDR) - " ${SOAPYSDR_INCLUDE_DIR}, ${SOAPYSDR_LIBRARY}, ${SOAPYSDR_DLL})
        endif()
    else()
        pkg_check_modules(SOAPYSDR SoapySDR)

        if(SOAPYSDR_FOUND)
            message(STATUS "SOAPYSDR: found - ${SOAPYSDR_INCLUDE_DIRS}, ${SOAPYSDR_LIBRARIES}")
            add_definitions(-DHASSOAPYSDR)
        endif()
    endif()
endif()

# Find RTL-SDR library.
if(RTLSDR)
    if(NOT MSVC)

        pkg_check_modules(PKG_RTLSDR librtlsdr)
        find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h
            HINTS ${PKG_RTLSDR_INCLUDE_DIRS} $ENV{RTLSDR_DIR}/include
            PATHS /usr/local/include /usr/include /opt/include /opt/local/include)

        if(RTLSDR_STATIC)
        # Look for static library first
        find_library(RTLSDR_LIBRARY 
                NAMES librtlsdr.a rtlsdr.a
                HINTS ${PKG_RTLSDR_LIBRARY_DIRS} $ENV{RTLSDR_DIR}/lib
                PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)
        else()
            # Look for shared library
            find_library(RTLSDR_LIBRARY rtlsdr
                HINTS ${PKG_RTLSDR_LIBRARY_DIRS} $ENV{RTLSDR_DIR}/lib
                PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)
        endif()


        if(RTLSDR_INCLUDE_DIR AND RTLSDR_LIBRARY)

            message(STATUS "RTLSDR: found - ${RTLSDR_INCLUDE_DIR}, ${RTLSDR_LIBRARY}")
            add_definitions(-DHASRTLSDR)

            set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY} ${LIBUSB_LIBRARY})
            if(RTLSDR_STATIC)
                message(STATUS "RTLSDR: using static linking")
            endif()

            include(CheckFunctionExists)
            set(CMAKE_REQUIRED_LIBRARIES ${RTLSDR_LIBRARIES})
            check_function_exists(rtlsdr_set_bias_tee HASRTLSDR_BIASTEE)
            if(HASRTLSDR_BIASTEE)
                add_definitions(-DHASRTLSDR_BIASTEE)
                Message(STATUS "RTLSDR: bias-tee support included.")
            else()
                Message(STATUS "RTLSDR: NO bias-tee support included.")
            endif()

            check_function_exists(rtlsdr_set_tuner_bandwidth HASRTLSDR_TUNERBW)
            if(HASRTLSDR_TUNERBW)
                add_definitions(-DHASRTLSDR_TUNERBW)
                Message(STATUS "RTLSDR: tuner badwidth support included.")
            else()
                Message(STATUS "RTLSDR: NO tuner bandwidth support included.")
            endif()

            unset(CMAKE_REQUIRED_LIBRARIES)

        else()
            Message(STATUS "RTLSDR: not found.")
        endif()

    elseif(MSVC_VCPKG)

        find_package(rtlsdr)
        if(rtlsdr_FOUND)
            add_definitions(-DHASRTLSDR)
            set(RTLSDR_LIBRARIES rtlsdr::rtlsdr)
            Message(STATUS "RTLSDR: found (VCPKG) - " ${RTLSDR_LIBRARIES})
        else()
            Message(STATUS "RTLSDR: not found (VCPKG).")
        endif()

    else()

        find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h HINTS ${POTHOSSDR_INCLUDE_DIR})
        find_library(RTLSDR_LIBRARY rtlsdr.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(RTLSDR_DLL rtlsdr.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(RTLSDR_INCLUDE_DIR AND RTLSDR_LIBRARY AND RTLSDR_DLL)
            Message(STATUS "RTLSDR: found (PothosSDR) - " ${RTLSDR_INCLUDE_DIR}, ${RTLSDR_LIBRARY}, ${RTLSDR_DLL})
            set(COPY_RTLSDR_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASRTLSDR)
            add_definitions(-DHASRTLSDR_BIASTEE)
            add_definitions(-DHASRTLSDR_TUNERBW)
            set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR})
            set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY})

            add_definitions(-DHASRTLSDR_BIASTEE)
            Message(STATUS "RTLSDR: bias-tee support included (assumed available).")
            add_definitions(-DHASRTLSDR_TUNERBW)
            Message(STATUS "RTLSDR: tuner badwidth support included (assumed available).")

        else()
            Message(STATUS "RTLSDR: not found (PothosSDR) - " ${RTLSDR_INCLUDE_DIR}, ${RTLSDR_LIBRARY}, ${RTLSDR_DLL})
        endif()
    endif()
endif()

# Find SDRplay library.
if(SDRPLAY)
    if(NOT MSVC)

        find_path(SDRPLAY_INCLUDE_DIR sdrplay_api.h)
        find_library(SDRPLAY_LIBRARY sdrplay_api)

        if(SDRPLAY_INCLUDE_DIR AND SDRPLAY_LIBRARY)
            message(STATUS "SDRPLAY: found - ${SDRPLAY_INCLUDE_DIR}, ${SDRPLAY_LIBRARY}")
            add_definitions(-DHASSDRPLAY)
            set(SDRPLAY_INCLUDE_DIRS ${SDRPLAY_INCLUDE_DIR})
            set(SDRPLAY_LIBRARIES ${SDRPLAY_LIBRARY})
        else()
            message(STATUS "SDRPLAY: not found.")
        endif()

    else()

        set(SDRPLAY_API_DIR $ENV{PROGRAMFILES}\\SDRplay\\API)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDRPLAY_API_LIB_DIR ${SDRPLAY_API_DIR}\\x64\\)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(SDRPLAY_API_LIB_DIR ${SDRPLAY_API_DIR}\\x86\\)
        endif()

        find_path(SDRPLAY_INCLUDE_DIR sdrplay_api.h HINTS ${SDRPLAY_API_DIR}\\inc)
        find_library(SDRPLAY_LIBRARY sdrplay_api.lib HINTS ${SDRPLAY_API_LIB_DIR})
        find_file(SDRPLAY_DLL sdrplay_api.dll HINTS ${SDRPLAY_API_LIB_DIR})

        if(SDRPLAY_INCLUDE_DIR AND SDRPLAY_LIBRARY AND SDRPLAY_DLL)
            message(STATUS "SDRPLAY: found - ${SDRPLAY_INCLUDE_DIR}, ${SDRPLAY_LIBRARY}, ${SDRPLAY_DLL}")
            add_definitions(-DHASSDRPLAY)
            set(COPY_SDRPLAY_DLL TRUE)
            set(SDRPLAY_INCLUDE_DIRS ${SDRPLAY_INCLUDE_DIR})
            set(SDRPLAY_LIBRARIES ${SDRPLAY_LIBRARY})
        else()
            message(STATUS "SDRPLAY: not found. Assumed directory ${SDRPLAY_API_LIB_DIR}")
        endif()

    endif()
endif()

# Find Airspy library.

if(AIRSPY)
    if(NOT MSVC)

        pkg_check_modules(PKG_AIRSPY libairspy)
        find_path(AIRSPY_INCLUDE_DIR airspy.h
            HINTS ${PKG_AIRSPY_INCLUDE_DIRS} $ENV{LIBAIRSPY_DIR}/include
            PATHS usr/local/include /usr/include)
        find_library(AIRSPY_LIBRARY airspy
            HINTS ${PKG_AIRSPY_LIBRARY_DIRS} $ENV{LIBAIRSPY_DIR}/lib
            PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)

        if(AIRSPY_INCLUDE_DIR AND AIRSPY_LIBRARY)

            message(STATUS "AIRSPY: found ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}")
            add_definitions(-DHASAIRSPY)
            set(AIRSPY_INCLUDE_DIRS ${AIRSPY_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(AIRSPY_LIBRARIES ${AIRSPY_LIBRARY} ${LIBUSB_LIBRARY})

        else()
            Message(STATUS "AIRSPY: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("AIRSPY: not found (VCPKG)")

    else()

        find_path(AIRSPY_INCLUDE_DIR airspy.h HINTS ${POTHOSSDR_INCLUDE_DIR} ${POTHOSSDR_INCLUDE_DIR}\\libairspy)
        find_library(AIRSPY_LIBRARY airspy.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(AIRSPY_DLL airspy.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(AIRSPY_INCLUDE_DIR AND AIRSPY_LIBRARY AND AIRSPY_DLL)
            Message(STATUS "AIRSPY: found (PothosSDR) - " ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}, ${AIRSPY_DLL})
            set(COPY_AIRSPY_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASAIRSPY)
            set(AIRSPY_INCLUDE_DIRS ${AIRSPY_INCLUDE_DIR})
            set(AIRSPY_LIBRARIES ${AIRSPY_LIBRARY})
        else()
            Message(STATUS "AIRSPY: not found (PothosSDR) - " ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}, ${AIRSPY_DLL})
        endif()
    endif()
endif()

# Find Airspy HFlibrary.

if(AIRSPYHF)
    if(NOT MSVC)

        pkg_check_modules(PKG_AIRSPYHF libairspyhf)
        find_path(AIRSPYHF_INCLUDE_DIR airspyhf.h HINTS ${PKG_AIRSPYHF_INCLUDE_DIRS})
        find_library(AIRSPYHF_LIBRARY airspyhf HINTS ${PKG_AIRSPYHF_LIBRARY_DIRS})

        if(AIRSPYHF_INCLUDE_DIR AND AIRSPYHF_LIBRARY)

            message(STATUS "AIRSPYHF: found - ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}")
            add_definitions(-DHASAIRSPYHF)
            set(AIRSPYHF_INCLUDE_DIRS ${AIRSPYHF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(AIRSPYHF_LIBRARIES ${AIRSPYHF_LIBRARY} ${LIBUSB_LIBRARY})

        else()
            Message(STATUS "AIRSPYHF: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("AIRSPYHF: not found (VCPKG)")

    else()

        find_path(AIRSPYHF_INCLUDE_DIR AIRSPYHF.h HINTS ${POTHOSSDR_INCLUDE_DIR} ${POTHOSSDR_INCLUDE_DIR}\\libairspyhf)
        find_library(AIRSPYHF_LIBRARY AIRSPYHF.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(AIRSPYHF_DLL AIRSPYHF.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(AIRSPYHF_INCLUDE_DIR AND AIRSPYHF_LIBRARY AND AIRSPYHF_DLL)
            Message(STATUS "AIRSPYHF: found (PothosSDR) - " ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}, ${AIRSPYHF_DLL})
            set(COPY_AIRSPYHF_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASAIRSPYHF)
            set(AIRSPYHF_INCLUDE_DIRS ${AIRSPYHF_INCLUDE_DIR})
            set(AIRSPYHF_LIBRARIES ${AIRSPYHF_LIBRARY})
        else()
            Message(STATUS "AIRSPYHF: NOT found (PothosSDR) - " ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}, ${AIRSPYHF_DLL})
        endif()

    endif()
endif()

# Find HACKRF library.
if(HACKRF)
    if(NOT MSVC)

        pkg_check_modules(PKG_HACKRF libhackrf)
        find_path(HACKRF_INCLUDE_DIR hackrf.h HINTS ${PKG_HACKRF_INCLUDE_DIRS} /usr/include/libhackrf)
        find_library(HACKRF_LIBRARY hackrf HINTS ${PKG_HACKRF_LIBRARY_DIRS})

        if(HACKRF_INCLUDE_DIR AND HACKRF_LIBRARY)

            message(STATUS "HACKRF: found - ${HACKRF_INCLUDE_DIR}, ${HACKRF_LIBRARY}")
            add_definitions(-DHASHACKRF)
            set(HACKRF_INCLUDE_DIRS ${HACKRF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(HACKRF_LIBRARIES ${HACKRF_LIBRARY} ${LIBUSB_LIBRARY})

        else()
            Message(STATUS "HACKRF: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("HACKRF: not found (VCPKG)")

    else()

        find_path(HACKRF_INCLUDE_DIR hackrf.h HINTS ${POTHOSSDR_INCLUDE_DIR} ${POTHOSSDR_INCLUDE_DIR}\\libhackrf)
        find_library(HACKRF_LIBRARY hackrf.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(HACKRF_DLL hackrf.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(HACKRF_INCLUDE_DIR AND HACKRF_LIBRARY AND HACKRF_DLL)
            Message(STATUS "HACKRF: found (PothosSDR) - " ${HACKRF_INCLUDE_DIR}, ${HACKRF_LIBRARY}, ${HACKRF_DLL})
            set(COPY_HACKRF_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASHACKRF)
            set(HACKRF_INCLUDE_DIRS ${HACKRF_INCLUDE_DIR})
            set(HACKRF_LIBRARIES ${HACKRF_LIBRARY})
        else()
            Message(STATUS "HACKRF: not found (PothosSDR)")
        endif()

    endif()
endif()

if(ZMQ)
    # Find libzmq
    if(NOT MSVC)
        pkg_check_modules(PKG_LIBZMQ libzmq)
        find_path(LIBZMQ_INCLUDE_DIR zmq.h HINTS ${PKG_LIBZMQ_INCLUDE_DIRS})
        find_library(LIBZMQ_LIBRARY zmq HINTS ${PKG_LIBZMQ_LIBRARY_DIRS})

        if(LIBZMQ_INCLUDE_DIR AND LIBZMQ_LIBRARY)
            message(STATUS "ZMQ: found - ${LIBZMQ_INCLUDE_DIR}, ${LIBZMQ_LIBRARY}")
            add_definitions(-DHASZMQ)
            set(ZMQ_INCLUDE_DIRS ${LIBZMQ_INCLUDE_DIR})
            set(ZMQ_LIBRARIES ${LIBZMQ_LIBRARY})
        else()
            message(STATUS "ZMQ: not found.")
        endif()

    elseif(MSVC_VCPKG)

        find_package(ZeroMQ)

        if(ZeroMQ_FOUND)
            add_definitions(-DHASZMQ)
            set(ZMQ_LIBRARIES libzmq)
            Message(STATUS "ZMQ: found (VCPKG) - " ${ZMQ_LIBRARIES})
        else()
            Message(STATUS "ZMQ: not found (VCPKG).")
        endif()

    else()
        find_path(ZMQ_INCLUDE_DIR zmq.h HINTS ${POTHOSSDR_INCLUDE_DIR})

        file(GLOB ZMQ_LIBRARY_CANDIDATES "${POTHOSSDR_LIBRARY_DIR}/libzmq*.lib")
        if(ZMQ_LIBRARY_CANDIDATES)
            list(GET ZMQ_LIBRARY_CANDIDATES 0 ZMQ_LIBRARY)
        endif()

        file(GLOB ZMQ_DLL_CANDIDATES "${POTHOSSDR_BINARY_DIR}/libzmq*.dll")
        if(ZMQ_DLL_CANDIDATES)
            list(GET ZMQ_DLL_CANDIDATES 0 ZMQ_DLL)
        endif()

        if(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARY AND ZMQ_DLL)
            Message(STATUS "ZMQ: found (PothosSDR) - " ${ZMQ_INCLUDE_DIR}, ${ZMQ_LIBRARY}, ${ZMQ_DLL})
            set(COPY_ZMQ_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASZMQ)
            set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR})
            set(ZMQ_LIBRARIES ${ZMQ_LIBRARY})
        else()
            Message(STATUS "ZMQ: NOT found (PothosSDR) - " ${ZMQ_INCLUDE_DIR}, ${ZMQ_LIBRARY}, ${ZMQ_DLL})
        endif()
    endif()
endif()

set(CPP
    Application/Main.cpp Application/Prometheus.cpp Application/WebViewer.cpp Application/Receiver.cpp Application/Config.cpp Tracking/Ships.cpp Tracking/DB.cpp DBMS/PostgreSQL.cpp Library/StackTracer.cpp
    Device/AIRSPYHF.cpp Device/FileWAV.cpp Device/RTLSDR.cpp Device/SDRPLAY.cpp DSP/Demod.cpp DSP/Model.cpp Library/AIS.cpp Library/JSONAIS.cpp Library/Keys.cpp
    Device/FileRAW.cpp Device/HACKRF.cpp Device/UDP.cpp Device/RTLTCP.cpp Device/ZMQ.cpp Device/SoapySDR.cpp Device/SpyServer.cpp Library/Message.cpp Library/NMEA.cpp
    Library/Utilities.cpp Library/TCP.cpp JSON/JSON.cpp IO/Network.cpp IO/HTTPServer.cpp JSON/StringBuilder.cpp JSON/Parser.cpp Library/Logger.cpp
    Device/AIRSPY.cpp Device/Serial.cpp IO/HTTPClient.cpp Application/WebDB.cpp
    DSP/DSP.cpp Device/N2KsktCAN.cpp Library/Basestation.cpp Library/Beast.cpp Library/ADSB.cpp
    IO/MsgOut.cpp IO/N2KStream.cpp Library/N2K.cpp IO/N2KInterface.cpp Protocol/Protocol.cpp)

set(HEADER
    Application/AIS-catcher.h Application/Prometheus.h Application/Config.h Application/WebDB.h Library/Logger.h Application/WebViewer.h Application/Receiver.h Tracking/Ships.h Tracking/DB.h DBMS/PostgreSQL.h IO/HTTPClient.h Application/MapTiles.h Library/Beast.h
    Device/Device.h Device/FileWAV.h Device/RTLTCP.h Device/UDP.h DSP/Demod.h DSP/Filters.h Library/AIS.h Library/Message.h Library/NMEA.h Library/ZIP.h Library/Signals.h Device/SoapySDR.h Library/JSONAIS.h JSON/JSON.h Library/Basestation.h Library/ADSB.h
    Device/AIRSPY.h Library/FIFO.h Device/N2KsktCAN.h Device/HACKRF.h Device/SDRPLAY.h DSP/DSP.h DSP/Model.h Tracking/History.h Tracking/Statistics.h Library/Common.h Library/Stream.h Device/SpyServer.h Library/Keys.h JSON/StringBuilder.h JSON/Parser.h Tracking/PlaneDB.h
    Device/Serial.h IO/N2KInterface.h Library/N2K.h IO/N2KStream.h Device/AIRSPYHF.h Device/FileRAW.h Device/RTLSDR.h Device/ZMQ.h DSP/FFT.h IO/MsgOut.h IO/Network.h IO/HTTPServer.h Library/Utilities.h Library/TCP.h Protocol/Protocol.h)

set(APP_INCLUDES . ./Tracking ./DBMS ./Library ./DSP ./Application ./IO ./Protocol)

# Executable

add_executable(AIS-catcher ${CPP} ${HEADER})

include_directories(
    . ${APP_INCLUDES} ${AIRSPYHF_INCLUDE_DIRS} ${NMEA2000_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${AIRSPY_INCLUDE_DIRS} ${HACKRF_INCLUDE_DIRS} ${RTLSDR_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS} ${SDRPLAY_INCLUDE_DIRS} ${SOAPYSDR_INCLUDE_DIRS} ${PQ_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIRS} ${PQXX_INCLUDE_DIRS} ${SOXR_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

target_link_libraries(AIS-catcher
    ${DL_LIBRARY} ${AIRSPY_LIBRARIES} ${NMEA2000_LIBRARIES} ${OPENSSL_LIBRARIES} ${AIRSPYHF_LIBRARIES} ${RTLSDR_LIBRARIES} ${HACKRF_LIBRARIES} ${ZMQ_LIBRARIES} ${PQ_LIBRARIES} ${SQLITE_LIBRARIES} ${PQXX_LIBRARIES} ${SDRPLAY_LIBRARIES} ${SOXR_LIBRARIES} ${SOAPYSDR_LIBRARIES} ${SAMPLERATE_LIBRARIES} ${CURL_LIBRARIES} ${ZLIB_LIBRARIES}
    ${ADDITIONAL_LIBRARIES} Threads::Threads)


# Copying DLLs to final location if needed
if(COPY_SDRPLAY_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SDRPLAY_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_RTLSDR_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${RTLSDR_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_AIRSPY_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${AIRSPY_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_AIRSPYHF_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${AIRSPYHF_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_OPENSSL_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${OPENSSL_DLL} ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CRYPTO_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_HACKRF_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${HACKRF_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_ZMQ_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${ZMQ_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_SOXR_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SOXR_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_PQ_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PQ_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_SQLITE_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SQLITE_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_CURL_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CURL_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_ZLIB1_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${ZLIB1_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_ZLIB_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${ZLIB_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_SAMPLERATE_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SAMPLERATE_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_SOAPYSDR_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SOAPYSDR_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_PTHREAD_LIBUSB_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${LIBUSB_DLL} ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PTHREAD_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Installation
install(TARGETS AIS-catcher DESTINATION bin)

# End of CMakeLists.txt
