cmake_minimum_required(VERSION 3.15)
project(test_deos_protocol_desktop CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../lib/deos_protocol deos_protocol_build)

add_executable(desktop_protocol_test test_deos_protocol_desktop.cpp)
target_link_libraries(desktop_protocol_test PRIVATE deos_protocol)
