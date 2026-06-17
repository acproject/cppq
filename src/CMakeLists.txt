# Core static library
add_library(cppq STATIC)

target_sources(
    cppq
    PRIVATE

    core/Column.cpp
    core/Table.cpp

    sql/Select.cpp
    sql/Insert.cpp
    sql/Update.cpp
    sql/Delete.cpp
    sql/Expression.cpp
)

# Only add pg sources if libpq is available
if(CPPQ_HAS_LIBPQ)
    target_sources(
        cppq
        PRIVATE
            pg/Connection.cpp
    )
endif()

target_include_directories(
    cppq
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_features(
    cppq
    PUBLIC
        cxx_std_23
)

# Link libpq if available
if(CPPQ_HAS_LIBPQ)
    target_include_directories(cppq PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(cppq PUBLIC ${PostgreSQL_LIBRARIES})
    target_compile_definitions(cppq PUBLIC CPPQ_HAS_LIBPQ=1)
endif()

# FFI shared library (for multi-language bindings)
if(CPPQ_BUILD_FFI AND CPPQ_HAS_LIBPQ)
    add_library(cppq_ffi SHARED)

    target_sources(
        cppq_ffi
        PRIVATE
            ffi/cppq_ffi.cpp
    )

    target_link_libraries(
        cppq_ffi
        PRIVATE
            cppq
    )

    target_compile_features(
        cppq_ffi
        PRIVATE
            cxx_std_23
    )

    # Install the shared library next to Python bindings for easy use
    set_target_properties(cppq_ffi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
endif()
