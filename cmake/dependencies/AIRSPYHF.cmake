# Find Airspy HFlibrary.
if(AIRSPYHF)
    if(NOT MSVC)

        pkg_check_modules(PKG_AIRSPYHF libairspyhf)
        find_path(AIRSPYHF_INCLUDE_DIR airspyhf.h HINTS ${PKG_AIRSPYHF_INCLUDE_DIRS})
        find_library(AIRSPYHF_LIBRARY airspyhf HINTS ${PKG_AIRSPYHF_LIBRARY_DIRS})

        if(AIRSPYHF_INCLUDE_DIR AND AIRSPYHF_LIBRARY)

            message(STATUS "AIRSPYHF: found - ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}")
            add_definitions(-DHASAIRSPYHF)
            set(AIRSPYHF_INCLUDE_DIRS ${AIRSPYHF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(AIRSPYHF_LIBRARIES ${AIRSPYHF_LIBRARY} ${LIBUSB_LIBRARY})

        else()
            Message(STATUS "AIRSPYHF: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("AIRSPYHF: not found (VCPKG)")

    else()

        find_path(AIRSPYHF_INCLUDE_DIR AIRSPYHF.h HINTS ${WINDOWS_INCLUDE_DIR} ${WINDOWS_INCLUDE_DIR}\\libairspyhf)
        find_library(AIRSPYHF_LIBRARY AIRSPYHF.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(AIRSPYHF_DLL AIRSPYHF.dll HINTS ${WINDOWS_BINARY_DIR})

        if(AIRSPYHF_INCLUDE_DIR AND AIRSPYHF_LIBRARY AND AIRSPYHF_DLL)
            Message(STATUS "AIRSPYHF: found (Windows) - " ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}, ${AIRSPYHF_DLL})
            set(COPY_AIRSPYHF_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASAIRSPYHF)
            set(AIRSPYHF_INCLUDE_DIRS ${AIRSPYHF_INCLUDE_DIR})
            set(AIRSPYHF_LIBRARIES ${AIRSPYHF_LIBRARY})
        else()
            Message(STATUS "AIRSPYHF: NOT found (Windows) - " ${AIRSPYHF_INCLUDE_DIR}, ${AIRSPYHF_LIBRARY}, ${AIRSPYHF_DLL})
        endif()

    endif()
endif()
