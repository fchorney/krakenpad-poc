# Fails the build if the bootloader image would overlap the boot-control
# record (FW_CTRL_OFFSET in ../common/fw_layout.h).
file(SIZE ${BIN} size)
if(size GREATER LIMIT)
    message(FATAL_ERROR "panel_bootloader.bin is ${size} bytes, over the ${LIMIT}-byte limit")
endif()
message(STATUS "panel_bootloader.bin: ${size} of ${LIMIT} bytes")
