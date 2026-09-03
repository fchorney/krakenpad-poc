cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_bringup C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(panel_bringup main.c)

# Shared with the main panel firmware — one copy of the PIO program.
pico_generate_pio_header(panel_bringup ${CMAKE_CURRENT_LIST_DIR}/../ws2812.pio)

target_link_libraries(panel_bringup
    pico_stdlib
    pico_unique_id
    hardware_adc
    hardware_pio
    hardware_flash
    hardware_sync
)

# W25Q32JV is 4MB. The SDK's default board header assumes 2MB, which would put
# the flash capacity test inside the aliasing window and make it always pass.
target_compile_definitions(panel_bringup PRIVATE PICO_FLASH_SIZE_BYTES=4194304)

pico_enable_stdio_usb(panel_bringup 1)
pico_enable_stdio_uart(panel_bringup 0)   # GPIO0/1 are the RS-485 bus, not a console

pico_add_extra_outputs(panel_bringup)
