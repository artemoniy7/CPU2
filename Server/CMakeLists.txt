cmake_minimum_required(VERSION 3.16)
project(CPU16Server LANGUAGES CXX)

include(CTest)

add_library(cpu16_server_network
    src/network/NetworkTypes.cpp
    src/network/VirtualNetwork.cpp
)
target_include_directories(cpu16_server_network PUBLIC include)
target_compile_features(cpu16_server_network PUBLIC cxx_std_17)

add_library(cpu16_server_vm
    src/vm/MachineConfig.cpp
    src/vm/Memory.cpp
    src/vm/DeviceBus.cpp
    src/vm/VirtualMachine.cpp
    src/vm/Cpu16Core.cpp
    src/storage/DiskImage.cpp
    src/manager/MachineCatalog.cpp
)
target_include_directories(cpu16_server_vm PUBLIC include)
target_compile_features(cpu16_server_vm PUBLIC cxx_std_17)

add_executable(cpu16-server src/manager/main.cpp)
target_link_libraries(cpu16-server PRIVATE cpu16_server_vm)
target_compile_features(cpu16-server PRIVATE cxx_std_17)

# The first graphical manager uses the native Windows desktop API. The
# portable server core and CLI remain available on Linux and other hosts.
if(WIN32)
    add_executable(cpu16-server-manager WIN32 src/manager/ServerManagerGui.cpp)
    target_link_libraries(cpu16-server-manager PRIVATE cpu16_server_vm comctl32 user32 gdi32)
    target_compile_features(cpu16-server-manager PRIVATE cxx_std_17)
    target_compile_definitions(cpu16-server-manager PRIVATE UNICODE _UNICODE)
endif()

if(MSVC)
    target_compile_options(cpu16_server_network PRIVATE /W4 /permissive-)
    target_compile_options(cpu16_server_vm PRIVATE /W4 /permissive-)
    target_compile_options(cpu16-server PRIVATE /W4 /permissive-)
    if(WIN32)
        target_compile_options(cpu16-server-manager PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(cpu16_server_network PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cpu16_server_vm PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cpu16-server PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(cpu16_server_network_tests tests/VirtualNetworkTests.cpp)
    target_link_libraries(cpu16_server_network_tests PRIVATE cpu16_server_network)
    add_test(NAME cpu16_server_network_tests COMMAND cpu16_server_network_tests)

    add_executable(cpu16_server_vm_tests tests/VirtualMachineTests.cpp)
    target_link_libraries(cpu16_server_vm_tests PRIVATE cpu16_server_vm)
    add_test(NAME cpu16_server_vm_tests COMMAND cpu16_server_vm_tests)

    add_executable(cpu16_server_cpu_tests tests/Cpu16CoreTests.cpp)
    target_link_libraries(cpu16_server_cpu_tests PRIVATE cpu16_server_vm)
    add_test(NAME cpu16_server_cpu_tests COMMAND cpu16_server_cpu_tests)

    add_executable(cpu16_server_manager_tests tests/ServerManagerTests.cpp)
    target_link_libraries(cpu16_server_manager_tests PRIVATE cpu16_server_vm)
    add_test(NAME cpu16_server_manager_tests COMMAND cpu16_server_manager_tests)
endif()
