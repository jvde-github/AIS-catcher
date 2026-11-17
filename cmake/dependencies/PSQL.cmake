# Find libpq
if(PSQL)

    if(MSVC AND NOT MSVC_VCPKG)
        find_path(PQ_INCLUDE_DIR libpq-fe.h HINTS ${WINDOWS_INCLUDE_DIR})
        find_library(PQ_LIBRARY libpq.lib HINTS ${WINDOWS_LIBRARY_DIR})
        find_file(PQ_DLL libpq.dll HINTS ${WINDOWS_BINARY_DIR})

        if(PQ_INCLUDE_DIR AND PQ_LIBRARY AND PQ_DLL AND OPENSSL_DLL AND CRYPTO_DLL)
            Message(STATUS "LIBPQ: found (Windows) - " ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}, ${PQ_DLL}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
            set(COPY_OPENSSL_DLL TRUE)
            set(COPY_PQ_DLL TRUE)
            add_definitions(-DHASPSQL)

            set(PQ_INCLUDE_DIRS ${PQ_INCLUDE_DIR})
            set(PQ_LIBRARIES ${PQ_LIBRARY})

        else()
            Message(STATUS "LIBPQ: Not found (Windows) - " ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}, ${PQ_DLL}, ${OPENSSL_DLL}, ${CRYPTO_DLL})
        endif()
    else()
        pkg_check_modules(PKG_PQ libpq)
        find_path(PQ_INCLUDE_DIR libpq-fe.h HINTS ${PKG_PQ_INCLUDE_DIRS})
        find_library(PQ_LIBRARY libpq.so HINTS ${PKG_PQ_LIBRARY_DIRS})
        find_library(PQ_LIBRARY libpq.dylib HINTS ${PKG_PQ_LIBRARY_DIRS})
        find_library(PQ_LIBRARY pq HINTS ${PKG_PQ_LIBRARY_DIRS})

        if(PQ_INCLUDE_DIR AND PQ_LIBRARY)

            message(STATUS "PQ: found - ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}")

            set(PQ_INCLUDE_DIRS ${PQ_INCLUDE_DIR})
            set(PQ_LIBRARIES ${PQ_LIBRARY})
            add_definitions(-DHASPSQL)
        else()
            message(STATUS "PQ: not found - ${PQ_INCLUDE_DIR}, ${PQ_LIBRARY}")
        endif()
    endif()
endif()
