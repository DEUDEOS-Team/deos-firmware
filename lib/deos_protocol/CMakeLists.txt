cmake_minimum_required(VERSION 3.15)
project(deos_protocol C)

set(DEOS_PROTOCOL_SOURCES
    src/deos_frame.c
    src/deos_codec.c
    src/deos_dispatcher.c
)

if(DEFINED ZEPHYR_BASE)
    add_library(deos_protocol STATIC ${DEOS_PROTOCOL_SOURCES})
    target_include_directories(deos_protocol PUBLIC include)
    target_link_libraries(deos_protocol PRIVATE zephyr_interface)
else()
    add_library(deos_protocol STATIC ${DEOS_PROTOCOL_SOURCES})
    target_include_directories(deos_protocol PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(deos_protocol PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
endif()
