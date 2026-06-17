cmake_minimum_required(VERSION 3.22)
project(
    cppq
    VERSION 0.1.0
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPPQ_BUILD_TESTS "Build tests" ON)
option(CPPQ_BUILD_EXAMPLES "Build examples" ON)
option(CPPQ_BUILD_FFI "Build FFI shared library for multi-language bindings" ON)

list(APPEND CMAKE_MODULE_PATH 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CompilerOptions)

# Find PostgreSQL (libpq)
# Hint for common install locations
list(APPEND CMAKE_PREFIX_PATH "/usr/local/pgsql" "/opt/homebrew/opt/libpq")
# Windows binary distribution hint
if(DEFINED PG_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${PG_ROOT}")
endif()
find_package(PostgreSQL QUIET)
if(NOT PostgreSQL_FOUND)
    # Fallback: try pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PostgreSQL QUIET libpq)
    endif()
endif()

if(NOT PostgreSQL_FOUND AND NOT PostgreSQL_INCLUDE_DIRS)
    # Manual fallback: look for libpq-fe.h in common locations
    find_path(PQ_INCLUDE_DIR libpq-fe.h
        PATHS
            /usr/local/pgsql/include
            /opt/homebrew/opt/libpq/include
            /usr/include/postgresql
            "${PG_ROOT}/include"
        NO_DEFAULT_PATH
    )
    if(PQ_INCLUDE_DIR)
        set(PostgreSQL_INCLUDE_DIRS ${PQ_INCLUDE_DIR})
        find_library(PQ_LIBRARY NAMES pq libpq
            PATHS
                /usr/local/pgsql/lib
                /opt/homebrew/opt/libpq/lib
                "${PG_ROOT}/lib"
            NO_DEFAULT_PATH
        )
        if(PQ_LIBRARY)
            set(PostgreSQL_LIBRARIES ${PQ_LIBRARY})
            set(CPPQ_HAS_LIBPQ ON)
        else()
            message(WARNING "libpq header found but library not found. FFI disabled.")
            set(CPPQ_HAS_LIBPQ OFF)
        endif()
    else()
        message(WARNING "libpq not found. Connection/Result layer and FFI will be disabled.")
        set(CPPQ_HAS_LIBPQ OFF)
    endif()
else()
    set(CPPQ_HAS_LIBPQ ON)
endif()

add_subdirectory(src)

if(CPPQ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CPPQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
