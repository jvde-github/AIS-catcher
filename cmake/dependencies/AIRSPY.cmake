# Find Airspy library.
if(AIRSPY)
    if(NOT MSVC)

        pkg_check_modules(PKG_AIRSPY libairspy)
        find_path(AIRSPY_INCLUDE_DIR airspy.h
            HINTS ${PKG_AIRSPY_INCLUDE_DIRS} $ENV{LIBAIRSPY_DIR}/include
            PATHS usr/local/include /usr/include)
        find_library(AIRSPY_LIBRARY airspy
            HINTS ${PKG_AIRSPY_LIBRARY_DIRS} $ENV{LIBAIRSPY_DIR}/lib
            PATHS /usr/local/lib /usr/lib /opt/lib /opt/local/lib)

        if(AIRSPY_INCLUDE_DIR AND AIRSPY_LIBRARY)

            message(STATUS "AIRSPY: found ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}")
            add_definitions(-DHASAIRSPY)
            set(AIRSPY_INCLUDE_DIRS ${AIRSPY_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR})
            set(AIRSPY_LIBRARIES ${AIRSPY_LIBRARY} ${LIBUSB_LIBRARY})

        else()
            Message(STATUS "AIRSPY: not found.")
        endif()

    elseif(MSVC_VCPKG)

        Message("AIRSPY: not found (VCPKG)")

    else()

        find_path(AIRSPY_INCLUDE_DIR airspy.h HINTS ${WINDOWS_INCLUDE_DIR} ${WINDOWS_INCLUDE_DIR}\\libairspy)
        find_library(AIRSPY_LIBRARY airspy.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(AIRSPY_DLL airspy.dll HINTS ${WINDOWS_BINARY_DIR})

        if(AIRSPY_INCLUDE_DIR AND AIRSPY_LIBRARY AND AIRSPY_DLL)
            Message(STATUS "AIRSPY: found (Windows) - " ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}, ${AIRSPY_DLL})
            set(COPY_AIRSPY_DLL TRUE)
            set(COPY_PTHREAD_LIBUSB_DLL TRUE)
            add_definitions(-DHASAIRSPY)
            set(AIRSPY_INCLUDE_DIRS ${AIRSPY_INCLUDE_DIR})
            set(AIRSPY_LIBRARIES ${AIRSPY_LIBRARY})
        else()
            Message(STATUS "AIRSPY: not found (Windows) - " ${AIRSPY_INCLUDE_DIR}, ${AIRSPY_LIBRARY}, ${AIRSPY_DLL})
        endif()
    endif()
endif()
