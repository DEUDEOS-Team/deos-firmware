cmake_minimum_required(VERSION 3.15)
project(deos_protocol C)

set(DEOS_PROTOCOL_SOURCES
    src/deos_frame.c
    src/deos_codec.c
    src/deos_registry.c
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

    # Paylaşımlı kütüphane (.so) - Python ctypes ve dinamik bağlayıcılar için
    add_library(deos_protocol_shared SHARED ${DEOS_PROTOCOL_SOURCES})
    set_target_properties(deos_protocol_shared PROPERTIES
        OUTPUT_NAME "deos_protocol"
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(deos_protocol_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()
