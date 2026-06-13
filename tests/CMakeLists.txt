# Unit tests (SQL builders, expressions - no database needed)
add_executable(test_sql_builder test_sql_builder.cpp)
target_link_libraries(test_sql_builder PRIVATE cppq)
target_compile_features(test_sql_builder PRIVATE cxx_std_23)
add_test(NAME sql_builder_tests COMMAND test_sql_builder)

# Integration tests (requires running PostgreSQL)
if(CPPQ_HAS_LIBPQ)
    add_executable(test_integration test_integration.cpp)
    target_link_libraries(test_integration PRIVATE cppq)
    target_compile_features(test_integration PRIVATE cxx_std_23)
    add_test(NAME integration_tests COMMAND test_integration)

    # JSON/JSONB tests
    add_executable(test_json test_json.cpp)
    target_link_libraries(test_json PRIVATE cppq)
    target_compile_features(test_json PRIVATE cxx_std_23)
    add_test(NAME json_tests COMMAND test_json)
endif()
