# Fails the build if an image is over the space it has in flash
# (../common/fw_layout.h).
file(SIZE ${BIN} size)
get_filename_component(name ${BIN} NAME)
if(size GREATER LIMIT)
    message(FATAL_ERROR "${name} is ${size} bytes, over the ${LIMIT}-byte limit")
endif()
message(STATUS "${name}: ${size} of ${LIMIT} bytes")
