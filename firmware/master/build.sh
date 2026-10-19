#!/bin/sh
# Build the master firmware with the KrakenPad USB type, and optionally upload.
#
#   firmware/master/build.sh                  compile
#   firmware/master/build.sh upload [port]    compile, then upload (port from
#                                             `arduino-cli board list` if omitted)
#
# Teensyduino has no hook for a custom HID descriptor, so this copies the
# installed Teensy 4 core into build/, applies core-patch/, and compiles against
# the copy. The installed core is never modified. A plain `arduino-cli compile`
# still builds, but with a warning and no pad input to the PC.
#
# The precompiled-header hook is disabled because it always reads the
# installed core, whose usb_desc.h does not know USB_KRAKENPAD.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
fqbn=teensy:avr:teensy40
ver=1.62.0                       # the Teensyduino release the patch is made against
patch_file=$here/core-patch/teensy4-$ver-krakenpad.patch
core=$(arduino-cli config get directories.data)/packages/teensy/hardware/avr/$ver/cores/teensy4
out=$here/build

if [ ! -d "$core" ]; then
    echo "Teensy core $ver not found at $core" >&2
    echo "Install teensy:avr@$ver, or remake the patch for the installed version." >&2
    exit 1
fi

rm -rf "$out/core"
mkdir -p "$out"
cp -R "$core" "$out/core"
patch -s -p1 -d "$out/core" < "$patch_file"

arduino-cli compile --fqbn "$fqbn" --build-path "$out/sketch" \
    --build-property "build.core.path=$out/core" \
    --build-property "build.usbtype=USB_KRAKENPAD" \
    --build-property "recipe.hooks.sketch.prebuild.1.pattern=true" \
    "$here"

if [ "${1:-}" = upload ]; then
    port=${2:-$(arduino-cli board list | awk '$2 == "teensy" { print $1; exit }')}
    if [ -z "$port" ]; then
        echo "No Teensy found by arduino-cli board list; pass the port" >&2
        exit 1
    fi
    arduino-cli upload --fqbn "$fqbn" -p "$port" --input-dir "$out/sketch" "$here"
fi
