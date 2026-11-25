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

set(VERSION_DESCRIBE "${VERSION_BASE}-${RUN_NUMBER}-g${COMMIT_HASH}")

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
# MINIMAL: for small devices, the radios, the decoder and the plain outputs; the options below
# default to off (an explicit -D<option>=ON still counts; in a fresh build directory)
option(MINIMAL "Small build: no web viewer, API, databases, TLS or resamplers" OFF)
if(MINIMAL)
    set(_full OFF)
else()
    set(_full ON)
endif()
option(SOXR "Include SOXR support" ${_full})
option(ZLIB "Include ZLIB support" ON)
option(SAMPLERATE "Include SAMPLERATE support" ${_full})
option(PSQL "Include PSQL support" ${_full})
option(SQLITE "Include SQLITE support" ${_full})
option(OPENSSL "Include OPENSSL support" ${_full})
option(WEBVIEWER "Include the embedded web UI (WebDB blob + HTTP server)" ${_full})
option(API "Include the tracker, its servers and /api/v1 (config keys tracking and services)" ${_full})
option(DELIVERY "Include webhooks and the MQTT state for Home Assistant (config keys webhooks and mqtt_state); needs API" ${_full})
option(STATIC "Fully static build: require and link static (.a) libraries for every enabled dependency" OFF)
option(RBIT "Use ARM RBIT instruction for bit reversal" OFF)

# the web viewer (-N and the ui service) reads the site's tracker
if(WEBVIEWER AND NOT API)
    message(STATUS "WEBVIEWER needs the tracker: API switched on")
    set(API ON)
endif()

foreach(_feature WEBVIEWER API RBIT)
    if(${_feature})
        add_definitions(-DHAS${_feature})
    endif()
endforeach()
if(API AND DELIVERY)
    add_definitions(-DHASDELIVERY)
endif()

set(PORTS "" CACHE PATH "Path to the libport directory (destination matching)")

# RPATH configuration for bundled libraries
# Set RPATH_LIBRARY_DIR to enable RPATH (e.g., -DRPATH_LIBRARY_DIR=/usr/lib/ais-catcher)
set(RPATH_LIBRARY_DIR "" CACHE STRING "Directory to add to RPATH for finding shared libraries")

if(RPATH_LIBRARY_DIR)
    message(STATUS "RPATH enabled: ${RPATH_LIBRARY_DIR}")
    set(CMAKE_BUILD_RPATH "${RPATH_LIBRARY_DIR}")
    set(CMAKE_INSTALL_RPATH "${RPATH_LIBRARY_DIR}")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

# Common compiler flags and options. Release unless a build type is given; Visual Studio picks its own.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MSVC_VCPKG "For MSVC use VCPKG libraries." OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SANITIZE "Build with AddressSanitizer + UBSanitizer" OFF)
option(TSAN "Build with ThreadSanitizer" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI/development only)" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # as compile options, after the Release flags, so -O1 is the one that counts
    if(SANITIZE)
        add_compile_options(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    elseif(TSAN)
        add_compile_options(-O1 -g -fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    else()
        set(OPTIMIZATION_FLAGS "-O3 -ffast-math")
        # stack canaries and checked libc calls; on Linux the relocation table is read-only as well.
        # A compiler that checks libc calls by itself, as Ubuntu's does at level 3, keeps its level
        if(NOT WIN32)
            add_compile_options(-fstack-protector-strong)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_FLAGS -O2)
            check_cxx_source_compiles("#ifndef _FORTIFY_SOURCE\n#error\n#endif\nint main() { return 0; }" FORTIFY_BY_DEFAULT)
            unset(CMAKE_REQUIRED_FLAGS)
            if(NOT FORTIFY_BY_DEFAULT)
                add_compile_options(-D_FORTIFY_SOURCE=2)
            endif()
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_link_options(-Wl,-z,relro,-z,now)
        endif()
    endif()
    # isFinite() lives there: with the rest's -ffast-math the compiler takes every number for finite
    set_source_files_properties(Source/Utilities/Helper.cpp PROPERTIES COMPILE_OPTIONS "-fno-finite-math-only")
    add_compile_options(-Wall -Wno-psabi -Wno-sign-compare)
    if(WARNINGS_AS_ERRORS)
        add_compile_options("-Werror")
    endif()
elseif(MSVC)
    if(WARNINGS_AS_ERRORS)
        add_compile_options("/WX")
    endif()
    set(OPTIMIZATION_FLAGS "-DWIN32_LEAN_AND_MEAN /EHsc /W3 /Oi /Ot /fp:fast") # spellchecker:disable-line
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZATION_FLAGS}")

include(FindPkgConfig)
include(CheckIncludeFile)
include(CheckSymbolExists)

find_package(Threads)

if(MSVC)
    add_definitions(-DHASEVENTLOG)
    set(ADDITIONAL_LIBRARIES ws2_32.lib crypt32.lib)
elseif(APPLE)
    check_include_file("os/log.h" HASOSLOG)
    if(HASOSLOG)
        add_definitions(-DHASOSLOG)
    endif()
elseif(UNIX)
    check_symbol_exists(syslog "syslog.h" CAN_USE_SYSLOG)
    if(CAN_USE_SYSLOG)
        add_definitions(-DHASSYSLOG)
    endif()
endif()

# Debug symbols and optimization settings for AIS-catcher
if(UNIX AND NOT APPLE)
    find_library(DL_LIBRARY dl REQUIRED)

    if(ENABLE_DEBUG)
        add_compile_options(-g3 -ggdb -O0 -fno-omit-frame-pointer)
        add_link_options(-rdynamic -no-pie)
        message(STATUS "Debug symbols enabled for Linux build via ENABLE_DEBUG")
    endif()
endif()

# MSVC finds each library's headers, .lib and DLL in -DWINDOWS_INCLUDE_DIR, -DWINDOWS_LIBRARY_DIR
# and -DWINDOWS_BINARY_DIR, as the Windows CI job fills them (cmake/dependencies/*.cmake)

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
    check_symbol_exists(__GLIBC__ "features.h" STATIC_ON_GLIBC)
    if(STATIC_ON_GLIBC)
        message(WARNING "STATIC on a glibc toolchain typically breaks NSS/DNS at runtime; build on musl (Alpine) for a clean fully-static binary.")
    endif()
    message(STATUS "STATIC: enabled - linking every enabled dependency from static archives (.a)")
endif()

foreach(_dep LibUSB OpenSSL ZLIB PSQL SQLite SOXR SAMPLERATE SoapySDR RTLSDR SDRPLAY AIRSPY AIRSPYHF HACKRF HYDRASDR)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/${_dep}.cmake)
endforeach()

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
    Source/Tracking/PlaceCatalogue.cpp
    Source/Tracking/PlaceGeometry.cpp
    Source/Site/Site.cpp
    Source/Site/SiteConfig.cpp
    Source/Control/ControlCore.cpp
    Source/Control/ControlServer.cpp
    Source/Application/DeviceManager.cpp
    Source/Application/Engine.cpp
    Source/Application/Stations.cpp
    Source/Application/CommandLine.cpp
    Source/Application/Usage.cpp
    Source/Application/Main.cpp
    Source/Control/ManagedMain.cpp
    Source/Web/MapTiles.cpp
    Source/Application/Receiver.cpp
    Source/IO/HTTPServer.cpp
    Source/Device/FileRAW.cpp
    Source/Device/FileWAV.cpp
    Source/Device/RTLTCP.cpp
    Source/Device/Serial.cpp
    Source/Device/SpyServer.cpp
    Source/Device/UDP.cpp
    Source/Device/TCPServerInput.cpp
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
    Source/Marine/N2K.cpp
    Source/DBMS/DatabaseOutput.cpp
    Source/DBMS/CSV.cpp
    Source/Library/Logger.cpp
    Source/IO/TCPServer.cpp
    Source/Tracking/DB.cpp
    Source/Tracking/Ships.cpp
    Source/Utilities/Parse.cpp
    Source/Utilities/Convert.cpp
    Source/Utilities/Helper.cpp
    Source/Utilities/TemplateString.cpp
    Source/Utilities/StreamHelpers.cpp
)

# Optional device sources: only compile when their backing library was found.
# Same condition that defines HAS<NAME> in cmake/dependencies/<NAME>.cmake.
foreach(_dev RTLSDR AIRSPY AIRSPYHF HACKRF HYDRASDR SDRPLAY SoapySDR)
    string(TOUPPER ${_dev} _lib)
    if(${_lib}_LIBRARIES)
        list(APPEND CPP Source/Device/${_dev}.cpp)
    endif()
endforeach()

if(PQ_LIBRARIES)
    list(APPEND CPP Source/DBMS/PostgreSQL.cpp)
endif()

if(SQLITE_LIBRARIES)
    list(APPEND CPP Source/DBMS/SQLite.cpp)
endif()

if(API)
    list(APPEND CPP
        Source/Tracking/Journal.cpp
        Source/Tracking/Gnss.cpp
        Source/Tracking/Health.cpp
        Source/Tracking/Tracker.cpp
        Source/Tracking/ViewFilter.cpp
        Source/Services/Http.cpp
        Source/Services/Server.cpp
        Source/Services/GeoJSON.cpp
        Source/Services/Kml.cpp
        Source/Services/Prometheus.cpp
        Source/Services/ApiV1.cpp
        Source/Services/Mcp.cpp
        Source/Services/Routes.cpp
        Source/Services/System.cpp)
    if(DELIVERY)
        list(APPEND CPP Source/Delivery/Webhooks.cpp Source/Delivery/MqttState.cpp)
    endif()
    if(WEBVIEWER)
        list(APPEND CPP Source/Services/Ui.cpp Source/Services/UiFormat.cpp Source/Services/Streams.cpp)
    endif()
endif()

# the pages the ui service and the control panel serve
if(WEBVIEWER)
    list(APPEND CPP
        Source/Web/WebDB.cpp
        Source/Web/PluginStore.cpp)
endif()

# headers only show in IDE projects
file(GLOB_RECURSE HEADER ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)

set(APP_INCLUDES . ./Source ./Source/Tracking ./Source/DBMS ./Source/Library ./Source/Marine ./Source/Aviation ./Source/DSP ./Source/Application ./Source/Web ./Source/Control ./Source/IO ./Source/JSON ./Source/Utilities ./Source/Site ./Source/Services ./Source/Delivery)


# A build on top of this one (a hub, a variant) adds its sources here
add_executable(AIS-catcher ${CPP} ${HEADER} ${EXTRA_SOURCES})
if(EXTRA_INCLUDES)
    target_include_directories(AIS-catcher PRIVATE ${EXTRA_INCLUDES})
endif()
if(EXTRA_DEFINES)
    target_compile_definitions(AIS-catcher PRIVATE ${EXTRA_DEFINES})
endif()

if(PORTS)
    add_subdirectory(${PORTS} ${CMAKE_CURRENT_BINARY_DIR}/libport)
    target_link_libraries(AIS-catcher port)
    target_compile_definitions(AIS-catcher PRIVATE HASPORTS)
    message(STATUS "PORTS: destination matching from ${PORTS}")
endif()

include_directories(
    ${APP_INCLUDES} ${AIRSPYHF_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${AIRSPY_INCLUDE_DIRS} ${HACKRF_INCLUDE_DIRS} ${HYDRASDR_INCLUDE_DIRS} ${RTLSDR_INCLUDE_DIRS} ${SDRPLAY_INCLUDE_DIRS} ${SOAPYSDR_INCLUDE_DIRS} ${PQ_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIRS} ${PQXX_INCLUDE_DIRS} ${SOXR_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

target_link_libraries(AIS-catcher
    ${DL_LIBRARY} ${AIRSPY_LIBRARIES} ${OPENSSL_LIBRARIES} ${AIRSPYHF_LIBRARIES} ${RTLSDR_LIBRARIES} ${HACKRF_LIBRARIES} ${HYDRASDR_LIBRARIES} ${PQ_LIBRARIES} ${SQLITE_LIBRARIES} ${PQXX_LIBRARIES} ${SDRPLAY_LIBRARIES} ${SOXR_LIBRARIES} ${SOAPYSDR_LIBRARIES} ${SAMPLERATE_LIBRARIES} ${ZLIB_LIBRARIES}
    ${ADDITIONAL_LIBRARIES} Threads::Threads)


# Copying DLLs to final location if needed
foreach(_dll SDRPLAY RTLSDR AIRSPY AIRSPYHF HACKRF HYDRASDR SOXR PQ SQLITE ZLIB SAMPLERATE SOAPYSDR)
    if(COPY_${_dll}_DLL)
        add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${${_dll}_DLL} ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endforeach()

if(COPY_OPENSSL_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${OPENSSL_DLL} ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CRYPTO_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(COPY_PTHREAD_LIBUSB_DLL)
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${LIBUSB_DLL} ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(TARGET AIS-catcher POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PTHREAD_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Installation
install(TARGETS AIS-catcher DESTINATION bin)
