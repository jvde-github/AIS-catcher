# Find libsoxr
if(SAMPLERATE)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SAMPLERATE_INCLUDE_DIR samplerate.h HINTS ${WINDOWS_INCLUDE_DIR})
        find_library(SAMPLERATE_LIBRARY samplerate.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(SAMPLERATE_DLL samplerate.dll HINTS ${WINDOWS_BINARY_DIR})

        if(SAMPLERATE_INCLUDE_DIR AND SAMPLERATE_LIBRARY AND SOXR_DLL)
            Message(STATUS "SAMPLERATE: found (Windows) - " ${SAMPLERATE_INCLUDE_DIR}, ${SAMPLERATE_LIBRARY}, ${SAMPLERATE_DLL})
            set(COPY_SAMPLERATE_DLL TRUE)
            add_definitions(-DHASSAMPLERATE)

            set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE_INCLUDE_DIR})
            set(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIBRARY})

        else()
            Message(STATUS "libsamplerate: NOT found (Windows) - " ${SAMPLERATE_INCLUDE_DIR}, ${SAMPLERATE_LIBRARY}, ${SAMPLERATE_DLL})
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
