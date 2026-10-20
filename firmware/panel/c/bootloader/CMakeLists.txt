cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(panel_bootloader C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(COMMON ${CMAKE_CURRENT_LIST_DIR}/../common)

# Builds, all from this directory (docs/FIELD_UPDATE.md):
#   stage0                 sector 0: boot2 + the slot selector
#   panel_bootloader_a/_b  the bootloader linked for slot A / slot B
#   panel_boot_usb.uf2     stage 0 + slot A, for a board on USB (BOOTSEL)
#   panel_boot_updater     app that installs all of it over the bus
#   panel_boot_test_fallback  bench test: forces stage 0 onto slot B

# ── Stage 0 ─────────────────────────────────────────────────────────────────
# No SDK runtime: boot2, a vector table and the selector, in under 4 KB.
# boot2 comes from the SDK's object library. Linking the library would add its
# object twice (once as an object library, once through its INTERFACE link).
add_executable(stage0 stage0.c $<TARGET_OBJECTS:bs2_default_library>)
add_dependencies(stage0 bs2_default_library)
target_include_directories(stage0 PRIVATE ${COMMON})
target_compile_options(stage0 PRIVATE -Os -ffreestanding -Wall -Wextra -Werror)
target_link_options(stage0 PRIVATE -nostdlib -nostartfiles
    "LINKER:--script=${CMAKE_CURRENT_LIST_DIR}/stage0.ld" "LINKER:-Map=stage0.elf.map")
add_custom_command(TARGET stage0 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:stage0> ${CMAKE_CURRENT_BINARY_DIR}/stage0.bin
    COMMAND ${CMAKE_COMMAND} -DBIN=${CMAKE_CURRENT_BINARY_DIR}/stage0.bin -DLIMIT=4096
            -P ${CMAKE_CURRENT_LIST_DIR}/check_size.cmake
    COMMAND ${CMAKE_COMMAND} -DBIN=${CMAKE_CURRENT_BINARY_DIR}/stage0.bin -DEXPECTED=EF89E9FE
            -DPYTHON=${Python3_EXECUTABLE} -P ${CMAKE_CURRENT_LIST_DIR}/check_stage0.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/stage0.bin
    VERBATIM)

# ── The bootloader, once per slot ───────────────────────────────────────────
# The SDK's copy_to_ram linker script with its flash region moved to the slot,
# as common/memmap_app.ld does for the app. The region stops short of the
# slot header, so a build that would overrun it fails to link.
set(SDK_COPY_TO_RAM_LD ${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2040/memmap_copy_to_ram.ld)
file(READ ${SDK_COPY_TO_RAM_LD} COPY_TO_RAM_LD)

function(panel_bootloader_slot NAME SLOT ORIGIN)
    set(region "FLASH(rx) : ORIGIN = ${ORIGIN}, LENGTH = 0x5F00")   # FW_SLOT_IMAGE_MAX
    string(REPLACE "INCLUDE \"pico_flash_region.ld\"" "${region}" ld "${COPY_TO_RAM_LD}")
    if (ld STREQUAL COPY_TO_RAM_LD)
        message(FATAL_ERROR "${SDK_COPY_TO_RAM_LD} no longer includes pico_flash_region.ld; "
                            "redo the slot linker script")
    endif()
    set(ld_file ${CMAKE_CURRENT_BINARY_DIR}/memmap_${NAME}.ld)
    file(WRITE ${ld_file} "/* Generated from ${SDK_COPY_TO_RAM_LD}: flash region = slot ${SLOT}. */\n${ld}")

    add_executable(${NAME} main.c)
    target_include_directories(${NAME} PRIVATE ${COMMON})
    target_link_libraries(${NAME} pico_stdlib hardware_flash hardware_uart hardware_irq hardware_watchdog)
    # Runs entirely from SRAM, so flash erase/program cannot stall the bus.
    pico_set_binary_type(${NAME} copy_to_ram)
    pico_set_linker_script(${NAME} ${ld_file})
    target_compile_definitions(${NAME} PRIVATE FW_SLOT=${SLOT} PICO_FLASH_SIZE_BYTES=4194304)
    # No console: GPIO0/1 are the bus.
    pico_enable_stdio_usb(${NAME} 0)
    pico_enable_stdio_uart(${NAME} 0)
    pico_add_bin_output(${NAME})
    pico_add_dis_output(${NAME})
    pico_add_map_output(${NAME})
endfunction()

panel_bootloader_slot(panel_bootloader_a 0 0x10001000)   # FW_SLOT_A_OFFSET
panel_bootloader_slot(panel_bootloader_b 1 0x10200000)   # FW_SLOT_B_OFFSET

# ── USB image: stage 0 + slot A, for a board in BOOTSEL ─────────────────────
set(USB_UF2 ${CMAKE_CURRENT_BINARY_DIR}/panel_boot_usb.uf2)
add_custom_command(OUTPUT ${USB_UF2}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/pack_boot.py
            ${CMAKE_CURRENT_BINARY_DIR}/stage0.bin
            ${CMAKE_CURRENT_BINARY_DIR}/panel_bootloader_a.bin
            ${CMAKE_CURRENT_BINARY_DIR}/panel_bootloader_b.bin ${USB_UF2}
    DEPENDS stage0 panel_bootloader_a panel_bootloader_b ${CMAKE_CURRENT_LIST_DIR}/pack_boot.py
    VERBATIM)
add_custom_target(panel_boot_usb ALL DEPENDS ${USB_UF2})

# ── Boot updater app ────────────────────────────────────────────────────────
set(STAGE0_BIN ${CMAKE_CURRENT_BINARY_DIR}/stage0.bin)
set(SLOT_A_BIN ${CMAKE_CURRENT_BINARY_DIR}/panel_bootloader_a.bin)
set(SLOT_B_BIN ${CMAKE_CURRENT_BINARY_DIR}/panel_bootloader_b.bin)
configure_file(updater_images.S.in ${CMAKE_CURRENT_BINARY_DIR}/updater_images.S @ONLY)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/updater_images.S PROPERTIES
    OBJECT_DEPENDS "${STAGE0_BIN};${SLOT_A_BIN};${SLOT_B_BIN}")

function(panel_boot_updater NAME)
    add_executable(${NAME} updater.c boot_install.c ${CMAKE_CURRENT_BINARY_DIR}/updater_images.S)
    add_dependencies(${NAME} stage0 panel_bootloader_a panel_bootloader_b)
    target_include_directories(${NAME} PRIVATE ${COMMON} ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(${NAME} pico_stdlib hardware_flash hardware_sync hardware_watchdog)
    pico_set_linker_script(${NAME} ${COMMON}/memmap_app.ld)   # an app at 0x10008000
    target_compile_definitions(${NAME} PRIVATE PICO_FLASH_SIZE_BYTES=4194304 ${ARGN})
    pico_enable_stdio_usb(${NAME} 1)
    pico_enable_stdio_uart(${NAME} 0)   # GPIO0/1 are the RS-485 bus
    pico_add_extra_outputs(${NAME})
endfunction()

panel_boot_updater(panel_boot_updater)
# Bench test only: wipes slot A's header so stage 0 must boot slot B.
panel_boot_updater(panel_boot_test_fallback UPDATER_TEST_FALLBACK=1)
