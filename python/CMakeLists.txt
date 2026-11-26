cmake_minimum_required(VERSION 3.15...3.27)
project(aiscat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# scikit-build-core 1.0+ sets Py_TARGET_ABI3T when wheel.py-api requests
# abi3t under a free-threaded interpreter.  Use that single signal for the
# source ABI, link target, extension suffix, and wheel tag so they cannot drift.
set(AISCAT_ABI3T OFF)
if(Py_TARGET_ABI3T)
    set(AISCAT_ABI3T ON)
    if(CMAKE_VERSION VERSION_LESS 3.26)
        message(FATAL_ERROR "AISCAT_ABI3T requires CMake 3.26 or newer")
    endif()
    find_package(Python REQUIRED COMPONENTS Interpreter Development.SABIModule)
    if(Python_VERSION VERSION_LESS 3.15)
        message(FATAL_ERROR "AISCAT_ABI3T requires Python 3.15 or newer")
    endif()
else()
    if(SKBUILD_SABI_VERSION)
        message(FATAL_ERROR
            "aiscat does not support the classic Stable ABI; build abi3t "
            "with free-threaded Python 3.15+ and wheel.py-api=cp315.cp315t")
    endif()
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
endif()

# Locate upstream Source/. Two valid layouts:
#   - git checkout: python/ is a sibling of Source/  -> use ../Source
#   - sdist:        Source/ has been vendored inside python/ by scripts/prepare_sdist.py
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Marine/NMEA.cpp)
    set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Source/Marine/NMEA.cpp)
    set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../Source)
else()
    message(FATAL_ERROR "Cannot find upstream Source/ — looked in ./Source and ../Source")
endif()
message(STATUS "Using upstream Source from ${SRC}")

set(AIS_SOURCES
    ${SRC}/Marine/AIS.cpp
    ${SRC}/Marine/Message.cpp
    ${SRC}/Marine/NMEA.cpp
    ${SRC}/JSON/JSON.cpp
    ${SRC}/JSON/JSONAIS.cpp
    ${SRC}/JSON/Keys.cpp
    ${SRC}/JSON/Parser.cpp
    ${SRC}/Library/Logger.cpp
    ${SRC}/Utilities/Convert.cpp
    ${SRC}/Utilities/Helper.cpp
    ${SRC}/Utilities/Parse.cpp
)

if(AISCAT_ABI3T)
    # USE_SABI selects Python::SABIModule.  This is required on Windows to
    # link against the stable python3 import library instead of python315.
    python_add_library(_core MODULE USE_SABI 3.15
        src/aiscat/_core.cpp
        ${AIS_SOURCES}
        WITH_SOABI
    )
else()
    python_add_library(_core MODULE
        src/aiscat/_core.cpp
        ${AIS_SOURCES}
        WITH_SOABI
    )
endif()

if(AISCAT_ABI3T)
    # Py_TARGET_ABI3T makes PyObject opaque and selects the free-threaded
    # Stable ABI.  Keep the normal build untouched: it remains the preferred
    # version-specific wheel for throughput-sensitive deployments.
    target_compile_definitions(_core PRIVATE Py_TARGET_ABI3T=0x030F0000)
    # CPython imports abi3t extensions as .abi3t.so on Unix, but as the
    # ordinary .pyd extension suffix on Windows.
    if(NOT WIN32)
        set_target_properties(_core PROPERTIES SUFFIX ".abi3t${CMAKE_SHARED_MODULE_SUFFIX}")
    endif()
endif()

target_include_directories(_core PRIVATE
    ${SRC}
    ${SRC}/Application
    ${SRC}/Library
    ${SRC}/Marine
    ${SRC}/JSON
    ${SRC}/Utilities
)

if(MSVC)
    target_compile_options(_core PRIVATE /W3 /wd4996)
else()
    target_compile_options(_core PRIVATE -Wno-unused-parameter -Wno-sign-compare -Wno-psabi)
endif()

install(TARGETS _core DESTINATION aiscat)

# Maintainer target: regenerate src/aiscat/types.py from msgs.json corpus.
# Requires the package to already be installed (e.g. `pip install -e .`).
# Not part of `all` — invoke explicitly: `cmake --build . --target regen-types`.
add_custom_target(regen-types
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_types.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating aiscat/types.py from msgs.json corpus"
    VERBATIM
)
