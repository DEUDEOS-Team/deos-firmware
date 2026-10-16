cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(deos_bms_gateway)

target_sources(app PRIVATE 
    src/main.cpp
    src/bms_gateway.cpp
    src/daly_parser.cpp

    ../../lib/deos_core/src/deos_core.c
    ../../lib/deos_core/src/deos_tx.c
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
