cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_bringup C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(panel_bringup main.c)
target_include_directories(panel_bringup PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../common)

# Linked at 0x10008000 to run under ../bootloader, which is what makes bus
# updates possible (docs/FIELD_UPDATE.md). Such an image does NOT boot on its
# own: load bootloader/build/panel_boot_usb.uf2 first. -DPANEL_UNDER_BOOTLOADER=OFF builds the
# old standalone image at 0x10000000.
option(PANEL_UNDER_BOOTLOADER "Link at the application offset, behind panel_bootloader" ON)
if (PANEL_UNDER_BOOTLOADER)
    pico_set_linker_script(panel_bringup ${CMAKE_CURRENT_LIST_DIR}/../common/memmap_app.ld)
    target_compile_definitions(panel_bringup PRIVATE PANEL_UNDER_BOOTLOADER=1)
endif()

# Shared with the main panel firmware — one copy of the PIO program.
pico_generate_pio_header(panel_bringup ${CMAKE_CURRENT_LIST_DIR}/../ws2812.pio)

target_link_libraries(panel_bringup
    pico_stdlib
    pico_unique_id
    hardware_adc
    hardware_pio
    hardware_flash
    hardware_sync
    hardware_uart
    hardware_irq
    hardware_watchdog
)

# W25Q32JV is 4MB. The SDK's default board header assumes 2MB, which would put
# the flash capacity test inside the aliasing window and make it always pass.
target_compile_definitions(panel_bringup PRIVATE PICO_FLASH_SIZE_BYTES=4194304)

pico_enable_stdio_usb(panel_bringup 1)
pico_enable_stdio_uart(panel_bringup 0)   # GPIO0/1 are the RS-485 bus, not a console

pico_add_extra_outputs(panel_bringup)
