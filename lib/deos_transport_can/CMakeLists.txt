add_library(deos_transport_can STATIC
    src/deos_can_transport.c
    src/deos_can_codec.c
)

target_include_directories(deos_transport_can PUBLIC include)
target_link_libraries(deos_transport_can PRIVATE zephyr_interface deos_core)
