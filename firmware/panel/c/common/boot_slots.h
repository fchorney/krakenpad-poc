// Bootloader slot checks, shared by stage 0, the boot updater and the host
// test, so all three agree on what "valid" means. Freestanding: stage 0 runs
// before any runtime, with no libc.
//
// `flash` is where offset 0 of the flash can be read (XIP_BASE on the panel, a
// buffer on the host); `link_base` is where it is mapped when code runs from it
// (FW_XIP_LINK_BASE), which the vector-table check needs.
#ifndef BOOT_SLOTS_H
#define BOOT_SLOTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fw_layout.h"

// Standard CRC-32 (zlib), a nibble at a time: a 64-byte table instead of 1 KB,
// small enough for stage 0 and fast enough to check a slot at boot.
// fw_crc32_from(fw_crc32(a), b) is the CRC-32 of a followed by b, as zlib's
// crc32(b, crc32(a)).
static inline uint32_t fw_crc32_from(uint32_t prev, const uint8_t *p, uint32_t n) {
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t c = ~prev;
    while (n--) {
        c ^= *p++;
        c = (c >> 4) ^ t[c & 0xFu];
        c = (c >> 4) ^ t[c & 0xFu];
    }
    return ~c;
}

static inline uint32_t fw_crc32(const uint8_t *p, uint32_t n) {
    return fw_crc32_from(0, p, n);
}

static inline uint32_t fw_slot_offset(int slot) {
    return slot ? FW_SLOT_B_OFFSET : FW_SLOT_A_OFFSET;
}

static inline const fw_slot_hdr_t *fw_slot_hdr(const uint8_t *flash, int slot) {
    return (const fw_slot_hdr_t *)(flash + fw_slot_offset(slot) + FW_SLOT_HDR_AT);
}

static inline uint32_t fw_slot_hdr_check(const fw_slot_hdr_t *h) {
    return fw_crc32((const uint8_t *)h, offsetof(fw_slot_hdr_t, check));
}

static inline bool fw_slot_hdr_ok(const fw_slot_hdr_t *h) {
    return h->magic == FW_SLOT_MAGIC && h->check == fw_slot_hdr_check(h) &&
           h->len >= 0x108u && h->len <= FW_SLOT_IMAGE_MAX;
}

// The header is sound, the image matches its CRC, and the vector table points
// into this slot's image the way boot2 expects.
static inline bool fw_slot_ok(const uint8_t *flash, uint32_t link_base, int slot) {
    const fw_slot_hdr_t *h = fw_slot_hdr(flash, slot);
    if (!fw_slot_hdr_ok(h)) return false;
    uint32_t off = fw_slot_offset(slot);
    const uint32_t *vt = (const uint32_t *)(flash + off + 0x100u);
    uint32_t sp = vt[0], pc = vt[1];
    uint32_t lo = link_base + off + 0x100u, hi = link_base + off + h->len;
    if (!(sp >= 0x20000000u && sp <= 0x20042000u && (sp & 3u) == 0)) return false;
    if (!((pc & 1u) && pc >= lo && pc < hi)) return false;
    return fw_crc32(flash + off, h->len) == h->crc;
}

// The slot stage 0 boots: the valid one with the higher sequence number,
// falling back to the other. -1 if neither is valid.
static inline int fw_slot_select(const uint8_t *flash, uint32_t link_base) {
    const fw_slot_hdr_t *a = fw_slot_hdr(flash, 0), *b = fw_slot_hdr(flash, 1);
    bool ha = fw_slot_hdr_ok(a), hb = fw_slot_hdr_ok(b);
    int first = (hb && (!ha || b->seq > a->seq)) ? 1 : 0;
    if (fw_slot_ok(flash, link_base, first)) return first;
    if (fw_slot_ok(flash, link_base, !first)) return !first;
    return -1;
}

static inline bool fw_stage0_present(const uint8_t *flash) {
    return *(const uint32_t *)(flash + FW_STAGE0_MARK_AT) == FW_STAGE0_MAGIC;
}

#endif
