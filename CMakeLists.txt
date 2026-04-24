cmake_minimum_required(VERSION 3.16)

project(ch32v208_mux_host
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

add_library(ch32v208_mux_host
    src/proto.c
    src/device.c
    src/uart.c
)

target_include_directories(ch32v208_mux_host
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ch32v208_mux_host
    PUBLIC
        PkgConfig::LIBUSB
)

target_compile_options(ch32v208_mux_host
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(ch32v208-mux-cli
    tools/ch32v208-mux-cli.c
)

target_link_libraries(ch32v208-mux-cli
    PRIVATE
        ch32v208_mux_host
)

if(BUILD_TESTING)
    add_executable(test_proto
        tests/test_proto.c
    )

    target_link_libraries(test_proto
        PRIVATE
            ch32v208_mux_host
    )

    add_test(NAME proto COMMAND test_proto)
endif()
