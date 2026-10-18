cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_bootloader C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(panel_bootloader main.c)
target_include_directories(panel_bootloader PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../common)

target_link_libraries(panel_bootloader
    pico_stdlib
    hardware_flash
    hardware_uart
    hardware_irq
    hardware_watchdog
)

# Runs entirely from SRAM, so flash erase/program cannot stall the bus.
pico_set_binary_type(panel_bootloader copy_to_ram)

target_compile_definitions(panel_bootloader PRIVATE PICO_FLASH_SIZE_BYTES=4194304)

# No console: GPIO0/1 are the bus, and USB would not fit in 28 KB.
pico_enable_stdio_usb(panel_bootloader 0)
pico_enable_stdio_uart(panel_bootloader 0)

pico_add_extra_outputs(panel_bootloader)

# The boot-control record starts at 0x7000. A bootloader that grows into it
# would be overwritten by the first install, so fail the build instead.
add_custom_command(TARGET panel_bootloader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE_DIR:panel_bootloader>/panel_bootloader.bin
            -DLIMIT=28672 -P ${CMAKE_CURRENT_LIST_DIR}/check_size.cmake
    VERBATIM)
