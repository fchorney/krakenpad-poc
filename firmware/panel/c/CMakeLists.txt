cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(panel_firmware main.c)

pico_generate_pio_header(panel_firmware ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(panel_firmware
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_pio
    hardware_clocks
)

pico_enable_stdio_usb(panel_firmware 1)
pico_enable_stdio_uart(panel_firmware 0)

pico_add_extra_outputs(panel_firmware)
