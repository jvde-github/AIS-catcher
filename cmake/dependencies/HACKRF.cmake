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

        find_path(HACKRF_INCLUDE_DIR hackrf.h HINTS ${WINDOWS_INCLUDE_DIR} ${WINDOWS_INCLUDE_DIR}\\libhackrf)
        find_library(HACKRF_LIBRARY hackrf.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(HACKRF_DLL hackrf.dll HINTS ${WINDOWS_BINARY_DIR})

        if(HACKRF_INCLUDE_DIR AND HACKRF_LIBRARY AND HACKRF_DLL)
            Message(STATUS "HACKRF: found (Windows) - " ${HACKRF_INCLUDE_DIR}, ${HACKRF_LIBRARY}, ${HACKRF_DLL})
            set(COPY_HACKRF_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASHACKRF)
            set(HACKRF_INCLUDE_DIRS ${HACKRF_INCLUDE_DIR})
            set(HACKRF_LIBRARIES ${HACKRF_LIBRARY})
        else()
            Message(STATUS "HACKRF: not found (Windows)")
        endif()

    endif()
endif()
