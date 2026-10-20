// Panel flash layout and bus-update constants, shared by the bootloader and
// every application that runs under it. docs/FIELD_UPDATE.md is the narrative.
//
// W25Q32JV, 4 MB, offsets from the start of flash (XIP_BASE = 0x10000000):
//
//   0x000000 - 0x000FFF    4 KB  stage 0: boot2 + bootloader slot selector
//   0x001000 - 0x006FFF   24 KB  bootloader slot A (header in its last page)
//   0x007000 - 0x007FFF    4 KB  boot-control record
//   0x008000 - 0x07FFFF  480 KB  application (its own boot2 copy at the
//                                start is unused; vectors at +0x100)
//   0x080000 - 0x0FFFFF  512 KB  released animation slot
//   0x100000 - 0x17FFFF  512 KB  pressed animation slot
//   0x180000 - 0x1FFFFF  512 KB  staging: an incoming image lands here
//   0x200000 - 0x205FFF   24 KB  bootloader slot B (header in its last page)
//   0x206000 - 0x27FFFF          golden copy (reserved, not implemented)
//   0x280000 - 0x3FFFFF          reserved
#ifndef FW_LAYOUT_H
#define FW_LAYOUT_H

// Where flash is mapped when the image runs: the address the app is linked
// against. Spelled out rather than taken from XIP_BASE so a host-side test
// build can map flash somewhere else.
#define FW_XIP_LINK_BASE   0x10000000u

// Before 2026-10-20 a single bootloader sat at 0x000000, up to 28 KB. Boards
// still on that layout are migrated by the boot updater (bootloader/updater.c).
#define FW_CTRL_OFFSET     0x007000u
#define FW_APP_OFFSET      0x008000u
#define FW_APP_SIZE        (480u * 1024u)
#define FW_STAGE_OFFSET    0x180000u
#define FW_STAGE_SIZE      (512u * 1024u)

// The application is linked with the SDK's standard layout shifted to
// FW_APP_OFFSET, so its 256-byte boot2 slot comes first and its vector table
// follows. The bootloader vectors through that table, exactly as boot2 would.
#define FW_APP_VTOR_OFFSET 0x100u

// ── Stage 0 and bootloader slots (docs/FIELD_UPDATE.md → "Updating the
// bootloader over the bus") ─────────────────────────────────────────────────
// Stage 0 is boot2 plus a selector. boot2 enters it through the two-word vector
// table at 0x100; the next word marks the sector as a stage 0, so the updater
// can tell it from an old single bootloader.
#define FW_STAGE0_OFFSET   0x000000u
#define FW_STAGE0_SIZE     0x001000u
#define FW_STAGE0_MARK_AT  0x108u
#define FW_STAGE0_MAGIC    0x30475453u   // "STG0"

// A slot holds one bootloader build linked for that slot's address, laid out
// like an app: an unused boot2 copy, then the vector table at +0x100. The
// header is the slot's last page and is written only after the image verifies.
#define FW_SLOT_A_OFFSET   0x001000u
#define FW_SLOT_B_OFFSET   0x200000u
#define FW_SLOT_SIZE       0x006000u
#define FW_SLOT_HDR_AT     (FW_SLOT_SIZE - 0x100u)
#define FW_SLOT_IMAGE_MAX  FW_SLOT_HDR_AT
#define FW_SLOT_MAGIC      0x544F4C53u   // "SLOT"

typedef struct {
    unsigned int magic;
    unsigned int seq;          // stage 0 boots the valid slot with the higher one
    unsigned int len;          // image bytes from the slot start
    unsigned int crc;          // CRC-32 of the image
    unsigned int tag;          // bootloader build tag reported in 'q'
    unsigned int check;        // CRC-32 of the fields above
} fw_slot_hdr_t;

// Written to watchdog scratch[0] by an application that wants the bootloader
// to stay resident after the reboot. Scratch registers survive a watchdog
// reset; the SDK only uses scratch[4..7].
#define FW_HOLD_MAGIC      0xB007C0DEu

// One 'D' block carries this many image bytes. 480 KB / 64 = 7680 blocks, so a
// block index fits in a u16.
#define FW_BLOCK_BYTES     64u
#define FW_MAX_BLOCKS      (FW_APP_SIZE / FW_BLOCK_BYTES)

// Bus commands (docs/RS485_PROTOCOL.md framing). Lowercase = reply.
#define FW_CMD_HOLD        'B'   // app: reboot into the bootloader; bootloader: stay
#define FW_CMD_HOLD_ACK    'b'
#define FW_CMD_BEGIN       'U'   // u32 len, u32 crc32
#define FW_CMD_DATA        'D'   // u16 block, <=64 bytes
#define FW_CMD_QUERY       'Q'
#define FW_CMD_QUERY_ACK   'q'
#define FW_CMD_ACTIVATE    'A'   // u32 crc32: install the staged image
#define FW_CMD_EXIT        'X'   // discard staging, run the application

// Bootloader states, reported in 'q'.
enum {
    FW_ST_IDLE      = 0,   // holding, nothing staged
    FW_ST_ERASING   = 1,   // never seen on the wire: it does not answer while erasing
    FW_ST_RECEIVING = 2,
    FW_ST_STAGED    = 3,   // every block in, CRC-32 matches
    FW_ST_CRC_FAIL  = 4,   // every block in, CRC-32 does not match
    FW_ST_REFUSED   = 5,   // 'U' asked for an image larger than FW_APP_SIZE
    FW_ST_INSTALLED = 6,   // copied to the app region and verified; 'X' runs it
    FW_ST_COPY_FAIL = 7,   // app region does not verify after the copy
};

// Boot-control record at FW_CTRL_OFFSET. Rewritten (erase + one page) only
// around an install, never during normal boots.
#define FW_CTRL_MAGIC      0x46574354u   // "FWCT"
typedef struct {
    unsigned int magic;
    unsigned int pending;      // 1 from before the app erase until the copy verifies
    unsigned int staged_len;
    unsigned int staged_crc;
    unsigned int app_len;      // 0 = unknown (app was loaded over USB or SWD)
    unsigned int app_crc;
    unsigned int check;        // CRC-32 of the fields above
} fw_ctrl_t;

#endif
