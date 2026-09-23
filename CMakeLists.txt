
cmake_minimum_required(VERSION 3.21)
project(portsplitter VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(portsplitter
    src/main.c
    src/config.c
)

if(MSVC)
    target_compile_options(portsplitter PRIVATE /W4 /WX /permissive- /utf-8)
    target_compile_definitions(portsplitter PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

enable_testing()

