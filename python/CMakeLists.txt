cmake_minimum_required(VERSION 3.15...3.27)
project(aiscat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

set(AIS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SRC ${AIS_ROOT}/Source)

set(AIS_SOURCES
    ${SRC}/Marine/AIS.cpp
    ${SRC}/Marine/Message.cpp
    ${SRC}/Marine/NMEA.cpp
    ${SRC}/JSON/JSON.cpp
    ${SRC}/JSON/JSONAIS.cpp
    ${SRC}/JSON/Keys.cpp
    ${SRC}/JSON/Parser.cpp
    ${SRC}/JSON/StringBuilder.cpp
    ${SRC}/Library/Logger.cpp
    ${SRC}/Utilities/Convert.cpp
    ${SRC}/Utilities/Helper.cpp
    ${SRC}/Utilities/Parse.cpp
    ${SRC}/Utilities/StreamHelpers.cpp
    ${SRC}/Utilities/TemplateString.cpp
)

python_add_library(_core MODULE
    src/aiscat/_core.cpp
    ${AIS_SOURCES}
    WITH_SOABI
)

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
    target_compile_options(_core PRIVATE -Wno-unused-parameter -Wno-sign-compare)
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
