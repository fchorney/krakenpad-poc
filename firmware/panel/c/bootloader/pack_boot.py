#!/usr/bin/env python3
"""Pack stage 0 + the slot A bootloader into one image for a board on USB or SWD.

    pack_boot.py STAGE0.bin SLOT_A.bin SLOT_B.bin OUT.uf2

Writes OUT.uf2 for `picotool load` (BOOTSEL), and OUT with .bin in place of
.uf2: flash 0x000000-0x006FFF, for SWD (`program ... 0x10000000`).

The UF2 also erases the sector holding slot B's header, so whatever slot B
held before cannot outrank the slot A written here. The .bin cannot: slot B
is above OpenOCD 0.11's 2 MB (docs/FIELD_UPDATE.md).

Layout and header: ../common/fw_layout.h. The tag in the header is the CRC-32
of the slot A build followed by the slot B build, as the boot updater writes
it. Slot B itself is not written.
"""

import struct
import sys
import zlib

STAGE0_SIZE = 0x1000
SLOT_A, SLOT_B, SLOT_SIZE = 0x001000, 0x200000, 0x6000
HDR_AT = SLOT_SIZE - 0x100
SLOT_MAGIC, STAGE0_MAGIC = 0x544F4C53, 0x30475453
XIP = 0x10000000
SECTOR, PAGE = 4096, 256

UF2_MAGIC0, UF2_MAGIC1, UF2_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
UF2_FLAG_FAMILY, RP2040_FAMILY = 0x00002000, 0xE48BFF56


def slot_header(image: bytes, seq: int, tag: int) -> bytes:
    fields = struct.pack("<5I", SLOT_MAGIC, seq, len(image), zlib.crc32(image), tag)
    return fields + struct.pack("<I", zlib.crc32(fields))


def uf2(chunks) -> bytes:
    """chunks: [(flash offset, bytes)], each a whole number of pages."""
    pages = [(off + i, data[i:i + PAGE]) for off, data in chunks for i in range(0, len(data), PAGE)]
    out = bytearray()
    for n, (off, page) in enumerate(pages):
        out += struct.pack("<8I", UF2_MAGIC0, UF2_MAGIC1, UF2_FLAG_FAMILY, XIP + off,
                           PAGE, n, len(pages), RP2040_FAMILY)
        out += page.ljust(476, b"\x00") + struct.pack("<I", UF2_END)
    return bytes(out)


def main() -> None:
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    stage0 = open(sys.argv[1], "rb").read()
    slot_a = open(sys.argv[2], "rb").read()
    slot_b = open(sys.argv[3], "rb").read()
    out_uf2 = sys.argv[4]
    tag = zlib.crc32(slot_b, zlib.crc32(slot_a))

    if len(stage0) > STAGE0_SIZE or struct.unpack_from("<I", stage0, 0x108)[0] != STAGE0_MAGIC:
        sys.exit(f"{sys.argv[1]}: not a stage 0 image")
    if len(slot_a) > HDR_AT:
        sys.exit(f"{sys.argv[2]}: {len(slot_a)} bytes, over the {HDR_AT}-byte slot")

    boot = bytearray(b"\xff" * (SLOT_A + SLOT_SIZE))
    boot[:len(stage0)] = stage0
    boot[SLOT_A:SLOT_A + len(slot_a)] = slot_a
    hdr = slot_header(slot_a, seq=1, tag=tag)
    boot[SLOT_A + HDR_AT:SLOT_A + HDR_AT + len(hdr)] = hdr

    open(out_uf2[:-4] + ".bin", "wb").write(boot)
    b_hdr_sector = (SLOT_B + HDR_AT) & ~(SECTOR - 1)
    open(out_uf2, "wb").write(uf2([(0, bytes(boot)), (b_hdr_sector, b"\xff" * SECTOR)]))
    print(f"{out_uf2}: stage 0 {len(stage0)} B + slot A {len(slot_a)} B, tag {tag:08X}")


if __name__ == "__main__":
    main()
