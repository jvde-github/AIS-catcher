# CMake definitions for AIS-catcher

# 3.2 floor keeps old distributions working (pre-3.12 CMake reads only the
# minimum); the 4.0 max suppresses deprecation warnings on newer CMake.
cmake_minimum_required(VERSION 3.2...4.0)

project(AIS-catcher)

# Get version from header file
file(STRINGS "${PROJECT_SOURCE_DIR}/Source/Application/AIS-catcher.h" VERSION_LINE REGEX "^#define VERSION[ \t]+\".*\"")
string(REGEX REPLACE "^#define VERSION[ \t]+\"(.*)\".*" "\\1" VERSION_BASE "${VERSION_LINE}")

# Run number from GitHub Actions (defaults to 0 for local builds)
set(RUN_NUMBER "0" CACHE STRING "GitHub Actions run number")

# Get commit hash
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(COMMIT_HASH "unknown")
endif()

# Build version string
if(RUN_NUMBER GREATER 0)
    set(VERSION_DESCRIBE "${VERSION_BASE}-${RUN_NUMBER}-g${COMMIT_HASH}")
else()
    set(VERSION_DESCRIBE "${VERSION_BASE}-0-g${COMMIT_HASH}")
endif()

message(STATUS "Version: ${VERSION_DESCRIBE} (base: ${VERSION_BASE}, run: ${RUN_NUMBER}, commit: ${COMMIT_HASH})")

# Define the version for compilation
add_definitions(-DVERSION_DESCRIBE="${VERSION_DESCRIBE}")

option(SOAPYSDR "Include Soapy SDR support" OFF)
option(RTLSDR "Include RTLSDR support" ON)
option(RTLSDR_STATIC "Statically link RTLSDR library" OFF)
option(AIRSPY "Include Airspy support" ON)
option(SDRPLAY "Include SDRPLAY support" ON)
option(AIRSPYHF "Include Airspy HF+ support" ON)
option(HACKRF "Include HackRF support" ON)
option(HYDRASDR "Include HydraSDR support" ON)
option(HYDRASDR_STATIC "Statically link HydraSDR library" OFF)
option(SOXR "Include SOXR support" ON)
option(ZLIB "Include ZLIB support" ON)
option(SAMPLERATE "Include SAMPLERATE support" ON)
option(ZMQ "Include ZMQ support" ON)
option(PSQL "Include PSQL support" ON)
option(SQLITE "Include SQLITE support" ON)
option(OPENSSL "Include OPENSSL support" ON)
option(NMEA2000 "Include NMEA2000 support" ON)
option(WEBVIEWER "Include the embedded web UI (WebDB blob + HTTP server)" ON)
option(STATIC "Fully static build: require and link static (.a) libraries for every enabled dependency" OFF)
option(ARMV6 "Compile for Raspberry Pi Zero" OFF)
option(RBIT "Use ARM RBIT instruction for bit reversal" OFF)

if(WEBVIEWER)
    add_definitions(-DHASWEBVIEWER)
endif()

if(RBIT)
    add_definitions(-DHASRBIT)
endif()

set(NMEA2000_PATH "." CACHE PATH "Path to NMEA2000 library")

# RPATH configuration for bundled libraries
# Set RPATH_LIBRARY_DIR to enable RPATH (e.g., -DRPATH_LIBRARY_DIR=/usr/lib/ais-catcher)
set(RPATH_LIBRARY_DIR "" CACHE STRING "Directory to add to RPATH for finding shared libraries")

if(RPATH_LIBRARY_DIR)
    message(STATUS "RPATH enabled: ${RPATH_LIBRARY_DIR}")
    set(CMAKE_BUILD_RPATH "${RPATH_LIBRARY_DIR}")
    set(CMAKE_INSTALL_RPATH "${RPATH_LIBRARY_DIR}")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
else()
    message(STATUS "RPATH not configured")
endif()

# Common compiler flags and options.
set(CMAKE_BUILD_TYPE Release)

option(MSVC_VCPKG "For MSVC use VCPKG libraries." OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SANITIZE "Build with AddressSanitizer + UBSanitizer" OFF)
option(TSAN "Build with ThreadSanitizer" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI/development only)" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(SANITIZE)
        set(OPTIMIZATION_FLAGS "-O1 -g")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    elseif(TSAN)
        set(OPTIMIZATION_FLAGS "-O1 -g")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    else()
        set(OPTIMIZATION_FLAGS "-O3 -ffast-math")
    endif()
    add_compile_options("-Wall")
    add_compile_options("-Wno-psabi")
    add_compile_options("-Wno-sign-compare")
    if(WARNINGS_AS_ERRORS)
        add_compile_options("-Werror")
    endif()
elseif(MSVC)
    if(WARNINGS_AS_ERRORS)
        add_compile_options("/WX")
    endif()
    set(OPTIMIZATION_FLAGS "-DWIN32_LEAN_AND_MEAN /EHsc /W3 /Oi /Ot /fp:fast") # spellchecker:disable-line
endif()

set(CMAKE_CXX_FLAGS "${OPTIMIZATION_FLAGS}")

include(FindPkgConfig)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)

# Detect non-zeroing std::string resize variants so JSON::Writer can avoid
# memset-on-resize in its hot path.
check_cxx_source_compiles("
    #include <string>
    int main() {
        std::string s;
        s.resize_and_overwrite(16, [](char*, std::size_t n){ return n; });
        return 0;
    }
" HAS_STD_RESIZE_AND_OVERWRITE)
if(HAS_STD_RESIZE_AND_OVERWRITE)
    add_definitions(-DHAS_STD_RESIZE_AND_OVERWRITE)
endif()

check_cxx_source_compiles("
    #include <string>
    int main() {
        std::string s;
        s.__resize_default_init(16);
        return 0;
    }
" HAS_LIBCXX_RESIZE_DEFAULT_INIT)
if(HAS_LIBCXX_RESIZE_DEFAULT_INIT)
    add_definitions(-DHAS_LIBCXX_RESIZE_DEFAULT_INIT)
endif()

check_cxx_source_compiles("
    #include <string>
    int main() {
        std::string s;
        s.__resize_and_overwrite(16, [](char*, std::size_t n){ return n; });
        return 0;
    }
" HAS_LIBSTDCXX_RESIZE_AND_OVERWRITE)
if(HAS_LIBSTDCXX_RESIZE_AND_OVERWRITE)
    add_definitions(-DHAS_LIBSTDCXX_RESIZE_AND_OVERWRITE)
endif()

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

# --- Static build setup (must run before the dependency modules are included) ---
if(STATIC)
    if(MSVC)
        message(FATAL_ERROR "STATIC build is not supported with MSVC.")
    endif()
    # Make generic find_library() calls in the dependency modules prefer static archives.
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    # Route the dependency modules that expose a static path through their static branch.
    set(RTLSDR_STATIC ON)
    set(HYDRASDR_STATIC ON)
    # Honored by find_package(OpenSSL)/find_package(ZLIB) where used.
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(ZLIB_USE_STATIC_LIBS TRUE)
    # Make pkg_check_modules resolve the full static dependency chain.
    set(PKG_CONFIG_USE_STATIC_LIBS ON)
    # Force a fully static executable.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER MATCHES "musl")
        message(WARNING "STATIC on a glibc toolchain typically breaks NSS/DNS at runtime; build on musl (Alpine) for a clean fully-static binary.")
    endif()
    message(STATUS "STATIC: enabled - linking every enabled dependency from static archives (.a)")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/NMEA2000.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/LibUSB.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/OpenSSL.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ZLIB.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/PSQL.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/SQLite.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/SOXR.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/SAMPLERATE.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/SoapySDR.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/RTLSDR.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/SDRPLAY.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/AIRSPY.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/AIRSPYHF.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/HACKRF.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/HYDRASDR.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ZMQ.cmake)

# --- Static build verification: every enabled dependency must resolve to a static (.a) archive ---
if(STATIC)
    # tag -> the resolved library path/list to check (only checked when the feature is enabled)
    set(_static_checks
        OPENSSL "${OPENSSL_LIBRARIES}"
        ZLIB    "${ZLIB_LIBRARIES}"
        RTLSDR  "${RTLSDR_LIBRARIES}"
        AIRSPY  "${AIRSPY_LIBRARIES}"
        AIRSPYHF "${AIRSPYHF_LIBRARIES}"
        HACKRF  "${HACKRF_LIBRARIES}"
        HYDRASDR "${HYDRASDR_LIBRARIES}"
        SDRPLAY "${SDRPLAY_LIBRARIES}"
        SOAPYSDR "${SOAPYSDR_LIBRARIES}"
        ZMQ     "${ZMQ_LIBRARIES}"
        SOXR    "${SOXR_LIBRARIES}"
        SAMPLERATE "${SAMPLERATE_LIBRARIES}"
        SQLITE  "${SQLITE_LIBRARIES}"
        PSQL    "${PQ_LIBRARIES}")
    while(_static_checks)
        list(POP_FRONT _static_checks _tag _libs)
        foreach(_lib IN LISTS _libs)
            # skip empty entries, imported targets, and bare -l/linker flags
            if(_lib AND NOT _lib MATCHES "^-" AND NOT TARGET ${_lib} AND EXISTS "${_lib}")
                if(NOT _lib MATCHES "\\.a$")
                    message(FATAL_ERROR
                        "STATIC build: ${_tag} resolved to a non-static library:\n  ${_lib}\n"
                        "Install its static (.a) package (e.g. openssl-libs-static / zlib-static / a static librtlsdr) "
                        "or disable this feature (-D${_tag}=OFF).")
                endif()
            endif()
        endforeach()
    endwhile()
    message(STATUS "STATIC: all enabled dependencies resolved to static archives.")
endif()

set(CPP
    Source/Application/Config.cpp
    Source/Control/ControlCore.cpp
    Source/Control/ControlServer.cpp
    Source/Application/DeviceManager.cpp
    Source/Application/Engine.cpp
    Source/Application/CommandLine.cpp
    Source/Application/Main.cpp
    Source/Control/ManagedMain.cpp
    Source/Web/MapTiles.cpp
    Source/Web/Prometheus.cpp
    Source/Application/Receiver.cpp
    Source/IO/HTTPServer.cpp
    Source/Device/FileRAW.cpp
    Source/Device/FileWAV.cpp
    Source/Device/RTLTCP.cpp
    Source/Device/Serial.cpp
    Source/Device/SpyServer.cpp
    Source/Device/UDP.cpp
    Source/DSP/Decoder/V2/V2Engine.cpp
    Source/DSP/Demod.cpp
    Source/DSP/DSP.cpp
    Source/DSP/Model.cpp
    Source/IO/HTTPClient.cpp
    Source/IO/MsgOut.cpp
    Source/IO/Screen.cpp
    Source/IO/Network.cpp
    Source/IO/Protocol.cpp
    Source/JSON/JSON.cpp
    Source/JSON/JSONAIS.cpp
    Source/JSON/Keys.cpp
    Source/JSON/Parser.cpp
    Source/Aviation/ADSB.cpp
    Source/Aviation/Basestation.cpp
    Source/Aviation/Beast.cpp
    Source/Marine/AIS.cpp
    Source/Marine/Message.cpp
    Source/Marine/NMEA.cpp
    Source/Library/Logger.cpp
    Source/IO/TCPServer.cpp
    Source/Tracking/DB.cpp
    Source/Tracking/Ships.cpp
    Source/Tracking/Region.cpp
    Source/Tracking/RegionTree.cpp
    Source/Utilities/Parse.cpp
    Source/Utilities/Convert.cpp
    Source/Utilities/Helper.cpp
    Source/Utilities/TemplateString.cpp
    Source/Utilities/StreamHelpers.cpp
)

# Optional device sources: only compile when their backing library was found.
# Same condition that defines HAS<NAME> in cmake/dependencies/<NAME>.cmake.
if(RTLSDR_LIBRARIES)
    list(APPEND CPP Source/Device/RTLSDR.cpp)
endif()
if(AIRSPY_LIBRARIES)
    list(APPEND CPP Source/Device/AIRSPY.cpp)
endif()
if(AIRSPYHF_LIBRARIES)
    list(APPEND CPP Source/Device/AIRSPYHF.cpp)
endif()
if(HACKRF_LIBRARIES)
    list(APPEND CPP Source/Device/HACKRF.cpp)
endif()
if(HYDRASDR_LIBRARIES)
    list(APPEND CPP Source/Device/HYDRASDR.cpp)
endif()
if(SDRPLAY_LIBRARIES)
    list(APPEND CPP Source/Device/SDRPLAY.cpp)
endif()
if(SOAPYSDR_LIBRARIES)
    list(APPEND CPP Source/Device/SoapySDR.cpp)
endif()
if(ZMQ_LIBRARIES)
    list(APPEND CPP Source/Device/ZMQ.cpp)
endif()
if(NMEA2000_LIBRARIES)
    list(APPEND CPP
        Source/Device/N2KsktCAN.cpp
        Source/IO/N2KInterface.cpp
        Source/IO/N2KStream.cpp
        Source/Marine/N2K.cpp)
endif()
# always built; must stay free of any driver type
list(APPEND CPP Source/DBMS/DatabaseOutput.cpp Source/DBMS/CSV.cpp)

if(PQ_LIBRARIES)
    list(APPEND CPP Source/DBMS/PostgreSQL.cpp)
endif()

if(SQLITE_LIBRARIES)
    list(APPEND CPP Source/DBMS/SQLite.cpp)
endif()

if(WEBVIEWER)
    list(APPEND CPP
        Source/Web/BackupManager.cpp
        Source/Web/FrontendConfig.cpp
        Source/Web/WebDB.cpp
        Source/Web/WebViewer.cpp
        Source/Tracking/ReceiverTracker.cpp)
endif()

set(HEADER
    Source/Application/AIS-catcher.h Source/Web/Prometheus.h Source/Application/Config.h Source/Application/DeviceManager.h Source/Web/WebDB.h Source/Library/Logger.h Source/Web/WebViewer.h Source/Application/Receiver.h Source/Tracking/Ships.h Source/Tracking/Region.h Source/Tracking/DB.h Source/Tracking/PathStore.h Source/Tracking/ReceiverTracker.h Source/Web/FrontendConfig.h Source/Web/BackupManager.h Source/DBMS/PostgreSQL.h Source/DBMS/DatabaseOutput.h Source/DBMS/SQLite.h Source/DBMS/CSV.h Source/IO/HTTPClient.h Source/Web/MapTiles.h Source/Aviation/Beast.h
    Source/Device/Device.h Source/Device/FileWAV.h Source/Device/RTLTCP.h Source/Device/UDP.h Source/DSP/Demod.h Source/DSP/Filters.h Source/Marine/AIS.h Source/Marine/Message.h Source/Marine/MessageHistory.h Source/Marine/NMEA.h Source/Library/ZIP.h Source/Library/Signals.h Source/Device/SoapySDR.h Source/JSON/JSONAIS.h Source/JSON/JSON.h Source/Aviation/Basestation.h Source/Aviation/ADSB.h
    Source/Device/AIRSPY.h Source/Library/FIFO.h Source/Device/N2KsktCAN.h Source/Device/HACKRF.h Source/Device/HYDRASDR.h Source/Device/SDRPLAY.h Source/DSP/DSP.h Source/DSP/Model.h Source/Tracking/History.h Source/Tracking/Statistics.h Source/Library/Common.h Source/Library/Stream.h Source/Library/SWAR.h Source/Device/SpyServer.h Source/JSON/Keys.h Source/JSON/Writer.h Source/JSON/Parser.h Source/Tracking/PlaneDB.h
    Source/Device/Serial.h Source/IO/N2KInterface.h Source/Marine/N2K.h Source/IO/N2KStream.h Source/Device/AIRSPYHF.h Source/Device/FileRAW.h Source/Device/RTLSDR.h Source/Device/ZMQ.h Source/DSP/FFT.h Source/IO/MsgOut.h Source/IO/Screen.h Source/IO/File.h Source/IO/StreamCounter.h Source/IO/Network.h Source/IO/HTTPServer.h Source/Utilities/StreamHelpers.h Source/IO/TCPServer.h Source/IO/Protocol.h
    Source/Utilities/Parse.h Source/Utilities/Convert.h Source/Utilities/Helper.h Source/Utilities/PackedInt.h Source/Utilities/TemplateString.h Source/IO/OutputStats.h)

set(APP_INCLUDES . ./Source ./Source/Tracking ./Source/DBMS ./Source/Library ./Source/Marine ./Source/Aviation ./Source/DSP ./Source/Application ./Source/Web ./Source/Control ./Source/IO ./Source/JSON ./Source/Utilities)

# Executable

# A build on top of this one (a hub, a variant) adds its sources here
add_executable(AIS-catcher ${CPP} ${HEADER} ${EXTRA_SOURCES})
if(EXTRA_INCLUDES)
    target_include_directories(AIS-catcher PRIVATE ${EXTRA_INCLUDES})
endif()
if(EXTRA_DEFINES)
    target_compile_definitions(AIS-catcher PRIVATE ${EXTRA_DEFINES})
endif()

include_directories(
    . ${APP_INCLUDES} ${AIRSPYHF_INCLUDE_DIRS} ${NMEA2000_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${AIRSPY_INCLUDE_DIRS} ${HACKRF_INCLUDE_DIRS} ${HYDRASDR_INCLUDE_DIRS} ${RTLSDR_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS} ${SDRPLAY_INCLUDE_DIRS} ${SOAPYSDR_INCLUDE_DIRS} ${PQ_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIRS} ${PQXX_INCLUDE_DIRS} ${SOXR_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

target_link_libraries(AIS-catcher
    ${DL_LIBRARY} ${AIRSPY_LIBRARIES} ${NMEA2000_LIBRARIES} ${OPENSSL_LIBRARIES} ${AIRSPYHF_LIBRARIES} ${RTLSDR_LIBRARIES} ${HACKRF_LIBRARIES} ${HYDRASDR_LIBRARIES} ${ZMQ_LIBRARIES} ${PQ_LIBRARIES} ${SQLITE_LIBRARIES} ${PQXX_LIBRARIES} ${SDRPLAY_LIBRARIES} ${SOXR_LIBRARIES} ${SOAPYSDR_LIBRARIES} ${SAMPLERATE_LIBRARIES} ${ZLIB_LIBRARIES}
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

if(COPY_HYDRASDR_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${HYDRASDR_DLL} ${CMAKE_CURRENT_BINARY_DIR})
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
