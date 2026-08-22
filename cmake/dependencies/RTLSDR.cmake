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
            if(STATIC)
                # A static probe must link the FULL transitive chain or the
                # feature test fails to link and reports a false negative.
                # librtlsdr's .pc lists a bare "Libs.private: -lusb-1.0" (not
                # "Requires.private: libusb-1.0"), so pkg-config does not recurse
                # into libusb's own deps (udev). Append libusb's static chain so
                # udev et al. are present for the probe link.
                set(CMAKE_REQUIRED_LIBRARIES ${PKG_RTLSDR_STATIC_LIBRARIES} ${PKG_LIBUSB_STATIC_LIBRARIES})
            else()
                set(CMAKE_REQUIRED_LIBRARIES ${RTLSDR_LIBRARIES})
            endif()
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
