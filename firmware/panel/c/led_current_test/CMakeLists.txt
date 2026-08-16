cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(led_current_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(led_current_test main.c)

# Shared with the panel firmware — one copy, no fork.
pico_generate_pio_header(led_current_test ${CMAKE_CURRENT_LIST_DIR}/../ws2812.pio)

target_link_libraries(led_current_test
    pico_stdlib
    hardware_pio
)

pico_enable_stdio_usb(led_current_test 1)
pico_enable_stdio_uart(led_current_test 0)

pico_add_extra_outputs(led_current_test)
