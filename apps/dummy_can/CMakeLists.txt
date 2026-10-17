cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE 
    "${CMAKE_CURRENT_LIST_DIR}/app.overlay"
    "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.overlay"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dummy_can)

target_sources(app PRIVATE
    src/main.c
    ../../lib/deos_core/src/deos_core.c
    ../../lib/deos_core/src/deos_send.c
    ../../lib/deos_core/src/deos_dispatch.c
    ../../lib/deos_core/src/deos_codec.c
    ../../lib/deos_core/src/deos_network.c
    ../../lib/deos_core/src/deos_router.c
    ../../lib/deos_core/src/deos_fault.c
    ../../lib/deos_core/src/deos_subscription.c
    ../../lib/deos_core/src/deos_frame.c
    ../../lib/deos_transport_can/src/deos_can_transport.c
    ../../lib/deos_transport_can/src/deos_can_codec.c
)

zephyr_include_directories(../../lib/deos_core/include)
zephyr_include_directories(../../lib/deos_transport_can/include)
