# Find zlib
if(ZLIB)
    if(MSVC)
        find_path(ZLIB_INCLUDE_DIR zlib.h HINTS ${WINDOWS_INCLUDE_DIR})
        # Upstream keeps renaming the shared library, and vcpkg installs it under upstream's name:
        # libz.dll/libz.lib from 1.4, z.dll/z.lib in 1.3.2, zlib1.dll/zlib.lib before. Newest first.
        find_library(ZLIB_LIBRARY NAMES libz z zlib zlibstatic HINTS ${WINDOWS_LIBRARY_DIR})
        # Pin to WINDOWS_BINARY_DIR; find_file's default search hits unrelated copies elsewhere
        # on the runner (e.g. Microsoft Service Fabric ships its own zlib1.dll) and breaks the copy.
        foreach(_zlib_dll libz.dll z.dll zlib1.dll)
            if(NOT ZLIB_DLL AND EXISTS "${WINDOWS_BINARY_DIR}/${_zlib_dll}")
                set(ZLIB_DLL "${WINDOWS_BINARY_DIR}/${_zlib_dll}")
            endif()
        endforeach()

        if(ZLIB_INCLUDE_DIR AND ZLIB_DLL)
            Message(STATUS "ZLIB: found (Windows) - " ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}, ${ZLIB_DLL})
            set(COPY_ZLIB_DLL TRUE)
            add_definitions(-DHASZLIB)

            set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
            set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
        else()
            Message(STATUS "ZLIB: not found (Windows) - " ${ZLIB_INCLUDE_DIR}, ${ZLIB_LIBRARY}, ${ZLIB_DLL})
        endif()

        add_definitions(-DHASZLIB)

    else()
        pkg_check_modules(PKG_ZLIB zlib)
        find_path(ZLIB_INCLUDE_DIR zlib.h HINTS ${PKG_ZLIB_INCLUDE_DIRS})
        if(STATIC)
            find_library(ZLIB_LIBRARY libz.a HINTS ${PKG_ZLIB_LIBRARY_DIRS})
        else()
            find_library(ZLIB_LIBRARY libz.so HINTS ${PKG_ZLIB_LIBRARY_DIRS})
            find_library(ZLIB_LIBRARY libz.dylib HINTS ${PKG_ZLIB_LIBRARY_DIRS})
            find_library(ZLIB_LIBRARY libz.tbd HINTS ${PKG_ZLIB_LIBRARY_DIRS})
            find_library(ZLIB_LIBRARY libz.dll HINTS ${PKG_ZLIB_LIBRARY_DIRS})
        endif()

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
