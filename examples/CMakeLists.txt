add_executable(simple_select 
    simple_select/main.cpp)

target_link_libraries(
    simple_select
    PRIVATE
        cppq
)