if (MSVC)
    add_compile_options(
        /W4 
        /permissive-
        /EHsc
    )

else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()
   