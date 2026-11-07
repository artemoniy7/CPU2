cmake_minimum_required(VERSION 3.16)
project(CPU16Server LANGUAGES CXX)

include(CTest)

add_library(cpu_server_core STATIC
    src/ServerShell.cpp
    src/ServerState.cpp
    src/ConfigManager.cpp
)

target_include_directories(cpu_server_core PUBLIC include)
target_compile_features(cpu_server_core PUBLIC cxx_std_17)

add_executable(cpu_server src/main.cpp)
target_link_libraries(cpu_server PRIVATE cpu_server_core)

function(cpu16_server_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

cpu16_server_enable_warnings(cpu_server_core)
cpu16_server_enable_warnings(cpu_server)

if(BUILD_TESTING)
    add_executable(cpu_server_tests tests/ServerTests.cpp)
    target_link_libraries(cpu_server_tests PRIVATE cpu_server_core)
    target_compile_features(cpu_server_tests PRIVATE cxx_std_17)
    cpu16_server_enable_warnings(cpu_server_tests)
    add_test(NAME server_configuration_and_logging COMMAND cpu_server_tests)
endif()