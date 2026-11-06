cmake_minimum_required(VERSION 3.16)
project(CPU16Server LANGUAGES CXX)

add_executable(cpu_server
    src/main.cpp
    src/ServerShell.cpp
    src/ServerState.cpp
)

target_include_directories(cpu_server PRIVATE include)
target_compile_features(cpu_server PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(cpu_server PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpu_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
