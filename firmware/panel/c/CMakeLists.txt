cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(panel_firmware main.c)
target_include_directories(panel_firmware PRIVATE ${CMAKE_CURRENT_LIST_DIR}/common)

# Linked at 0x10008000 to run under bootloader/, like the bring-up build: load
# panel_bootloader.uf2 first, or update over the bus (docs/FIELD_UPDATE.md).
# -DPANEL_UNDER_BOOTLOADER=OFF builds a standalone image at 0x10000000.
option(PANEL_UNDER_BOOTLOADER "Link at the application offset, behind panel_bootloader" ON)
if (PANEL_UNDER_BOOTLOADER)
    pico_set_linker_script(panel_firmware ${CMAKE_CURRENT_LIST_DIR}/common/memmap_app.ld)
    target_compile_definitions(panel_firmware PRIVATE PANEL_UNDER_BOOTLOADER=1)
endif()

pico_generate_pio_header(panel_firmware ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(panel_firmware
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_dma
    hardware_pio
    hardware_uart
    hardware_irq
    hardware_watchdog
)

# W25Q32JV is 4MB; the SDK's default board header assumes 2MB.
target_compile_definitions(panel_firmware PRIVATE PICO_FLASH_SIZE_BYTES=4194304)

pico_enable_stdio_usb(panel_firmware 1)
pico_enable_stdio_uart(panel_firmware 0)   # GPIO0/1 are the RS-485 bus

pico_add_extra_outputs(panel_firmware)
