# Stage 0 is written once per board and never again: the updater refuses a
# board whose sector 0 differs from the stage 0 it carries. So a build whose
# stage 0 bytes changed would strand every migrated board from bootloader
# updates. Fail the build instead. Changing stage 0 on purpose is a separate
# decision (docs/FIELD_UPDATE.md); only then update EXPECTED.
execute_process(COMMAND ${PYTHON} -c "import zlib,sys;print('%08X'%zlib.crc32(open(sys.argv[1],'rb').read()))" ${BIN}
                OUTPUT_VARIABLE crc OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT crc STREQUAL EXPECTED)
    message(FATAL_ERROR "stage0.bin changed: CRC-32 ${crc}, expected ${EXPECTED}. "
                        "Boards already migrated keep their stage 0, and the updater refuses them.")
endif()
message(STATUS "stage0.bin: CRC-32 ${crc}, unchanged")
