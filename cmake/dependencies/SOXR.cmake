# Find libsoxr
if(SOXR)
    if(MSVC AND NOT MSVC_VCPKG)
        find_path(SOXR_INCLUDE_DIR soxr.h HINTS ${WINDOWS_INCLUDE_DIR})
        find_library(SOXR_LIBRARY soxr.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(SOXR_DLL soxr.dll HINTS ${WINDOWS_BINARY_DIR})

        if(SOXR_INCLUDE_DIR AND SOXR_LIBRARY AND SOXR_DLL)
            Message(STATUS "SOXR: found (Windows) - " ${SOXR_INCLUDE_DIR}, ${SOXR_LIBRARY}, ${SOXR_DLL})
            set(COPY_SOXR_DLL TRUE)
            add_definitions(-DHASSOXR)

            set(SOXR_INCLUDE_DIRS ${SOXR_INCLUDE_DIR})
            set(SOXR_LIBRARIES ${SOXR_LIBRARY})

        else()
            Message(STATUS "SOXR: NOT found (Windows) - " ${SOXR_INCLUDE_DIR}, ${SOXR_LIBRARY}, ${SOXR_DLL})
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
