cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE 
    "${CMAKE_CURRENT_LIST_DIR}/app.overlay"
    "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.overlay"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(deos_firmware)


target_sources(app PRIVATE
    src/main.cpp
    src/MainApp.cpp
    src/io/LedController.cpp
    src/io/SafetyManager.cpp
    src/io/ImuSensor.cpp
    src/network/NetworkMonitor.cpp
    src/transport/eth_textual.cpp
    src/transport/eth_ros_bridge.cpp
    src/transport/uart_lora.cpp
    ../../lib/deos_core/src/deos_core.c
    ../../lib/deos_core/src/deos_rx.c
    ../../lib/deos_core/src/deos_tx.c
    ../../lib/deos_core/src/deos_dispatch.c
    ../../lib/deos_core/src/deos_codec.c
    ../../lib/deos_core/src/deos_network.c
    ../../lib/deos_core/src/deos_router.c
    ../../lib/deos_core/src/deos_fault.c
    ../../lib/deos_core/src/deos_subscription.c
)

zephyr_include_directories(../../lib/deos_core/include)

# Disable thread-safe statics for C++ to prevent __cxa_guard_* linker errors
target_compile_options(app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)

# Disable thread-safe statics for C++ to prevent __cxa_guard_* linker errors
target_compile_options(app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
