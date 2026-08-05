# Find HydraSDR library.
if(HYDRASDR)
    if(NOT MSVC)

        pkg_check_modules(PKG_HYDRASDR libhydrasdr)
        find_path(HYDRASDR_INCLUDE_DIR hydrasdr.h
            HINTS ${PKG_HYDRASDR_INCLUDE_DIRS} $ENV{HYDRASDR_DIR}/include
            PATHS /usr/local/include /usr/include /opt/include /opt/local/include /usr/include/libhydrasdr)

        if(HYDRASDR_STATIC)
            # Look for static library first
            find_library(HYDRASDR_LIBRARY
                NAMES libhydrasdr.a hydrasdr.a
                HINTS ${PKG_HYDRASDR_LIBRARY_DIRS} $ENV{HYDRASDR_DIR}/lib
                PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)
        else()
            # Look for shared library
            find_library(HYDRASDR_LIBRARY hydrasdr
                HINTS ${PKG_HYDRASDR_LIBRARY_DIRS} $ENV{HYDRASDR_DIR}/lib
                PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)
        endif()

        if(HYDRASDR_INCLUDE_DIR AND HYDRASDR_LIBRARY)

            # the driver needs the unified gain API introduced in libhydrasdr 1.1
            file(STRINGS ${HYDRASDR_INCLUDE_DIR}/hydrasdr.h HYDRASDR_UNIFIED_GAIN_API REGEX "HYDRASDR_VERSION_NUM")
            if(NOT HYDRASDR_UNIFIED_GAIN_API)
                message(WARNING "HYDRASDR: library at ${HYDRASDR_INCLUDE_DIR} is older than 1.1, HydraSDR support disabled. Update libhydrasdr to enable.")
            else()
                message(STATUS "HYDRASDR: found - ${HYDRASDR_INCLUDE_DIR}, ${HYDRASDR_LIBRARY}")
                add_definitions(-DHASHYDRASDR)
                set(HYDRASDR_INCLUDE_DIRS ${HYDRASDR_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
                set(HYDRASDR_LIBRARIES ${HYDRASDR_LIBRARY} ${LIBUSB_LIBRARY})
                if(HYDRASDR_STATIC)
                    message(STATUS "HYDRASDR: using static linking")
                endif()
            endif()

        else()
            Message(STATUS "HYDRASDR: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("HYDRASDR: not found (VCPKG)")

    else()

        find_path(HYDRASDR_INCLUDE_DIR hydrasdr.h HINTS ${POTHOSSDR_INCLUDE_DIR} ${POTHOSSDR_INCLUDE_DIR}\\libhydrasdr)
        find_library(HYDRASDR_LIBRARY hydrasdr.lib HINTS ${POTHOSSDR_LIBRARY_DIR})
        find_file(HYDRASDR_DLL hydrasdr.dll HINTS ${POTHOSSDR_BINARY_DIR})

        if(HYDRASDR_INCLUDE_DIR AND HYDRASDR_LIBRARY AND HYDRASDR_DLL)
            Message(STATUS "HYDRASDR: found (PothosSDR) - " ${HYDRASDR_INCLUDE_DIR}, ${HYDRASDR_LIBRARY}, ${HYDRASDR_DLL})
            set(COPY_HYDRASDR_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASHYDRASDR)
            set(HYDRASDR_INCLUDE_DIRS ${HYDRASDR_INCLUDE_DIR})
            set(HYDRASDR_LIBRARIES ${HYDRASDR_LIBRARY})
        else()
            Message(STATUS "HYDRASDR: not found (PothosSDR)")
        endif()

    endif()
endif()
