cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dummy_can)

target_sources(app PRIVATE
    src/main.c
    ../../lib/deos_core/src/deos_core.c
    ../../lib/deos_core/src/deos_rx.c
    ../../lib/deos_core/src/deos_tx.c
    ../../lib/deos_core/src/deos_dispatch.c
    ../../lib/deos_core/src/deos_codec.c
    ../../lib/deos_core/src/deos_network.c
    ../../lib/deos_core/src/deos_router.c
    ../../lib/deos_core/src/deos_fault.c
    ../../lib/deos_core/src/deos_subscription.c
    ../../lib/deos_core/src/deos_frame.c
)

zephyr_include_directories(../../lib/deos_core/include)
