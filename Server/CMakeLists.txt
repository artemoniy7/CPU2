cmake_minimum_required(VERSION 3.16)
project(CPU16Server LANGUAGES CXX)

include(CTest)

add_library(cpu16_server_network
    src/network/NetworkTypes.cpp
    src/network/VirtualNetwork.cpp
)
target_include_directories(cpu16_server_network PUBLIC include)
target_compile_features(cpu16_server_network PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(cpu16_server_network PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpu16_server_network PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(cpu16_server_network_tests
        tests/VirtualNetworkTests.cpp
    )
    target_link_libraries(cpu16_server_network_tests PRIVATE cpu16_server_network)
    add_test(NAME cpu16_server_network_tests COMMAND cpu16_server_network_tests)
endif()
