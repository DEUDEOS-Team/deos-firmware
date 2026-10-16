add_library(deos_core STATIC
    src/deos_core.c
    src/deos_tx.c
    src/deos_dispatch.c
    src/deos_codec.c
    src/deos_network.c
    src/deos_router.c
    src/deos_fault.c
    src/deos_subscription.c
    src/deos_frame.c
)
target_include_directories(deos_core PUBLIC include)
target_link_libraries(deos_core PRIVATE zephyr_interface)
