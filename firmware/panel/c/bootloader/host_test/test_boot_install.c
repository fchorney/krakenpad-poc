// Power-cut test of the boot updater's write order (../boot_install.c), on the
// host, against the real built images.
//
//   test_boot_install STAGE0.bin SLOT_A.bin SLOT_B.bin LEGACY.bin USB.bin
//
// For every flash operation the updater makes (one sector erase or one page
// program), it cuts power at that operation, leaving it torn: an erase leaves
// random bytes, a program clears a random subset of the bits it meant to. Then
// it decides what the board does at the next power-up, the way the boot ROM and
// stage 0 would, and fails on any outcome other than:
//   - BOOTSEL, because boot2's CRC fails (recoverable over USB), or
//   - booting a complete bootloader: the old single one, or a valid slot.
// A board that boots runs the updater again (the app region is never touched),
// so the test resumes it with no cut and checks the end state. A board in
// BOOTSEL is recovered the way the doc says, with USB.bin, then resumed.
//
// Scenarios: migrating an old board, updating a migrated one, an up-to-date
// board, and a sector 0 holding some other stage 0.

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "boot_slots.h"
#include "../boot_install.h"

#define FLASH_BYTES (4u * 1024u * 1024u)
#define SECTOR 4096u
#define PAGE   256u
#define LINK   FW_XIP_LINK_BASE

static uint8_t flash[FLASH_BYTES];
static long ops, cut_at;              // cut_at < 0: no cut
static jmp_buf cut;
static unsigned rng = 1;

static unsigned rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

static void op(void) {
    if (cut_at >= 0 && ops == cut_at) longjmp(cut, 1);
    ops++;
}

static void sim_erase(uint32_t off, uint32_t len) {
    if (off % SECTOR || len % SECTOR || off + len > FLASH_BYTES) { puts("BAD ERASE"); exit(1); }
    for (uint32_t s = off; s < off + len; s += SECTOR) {
        if (cut_at >= 0 && ops == cut_at)                 // torn: anything at all
            for (uint32_t i = 0; i < SECTOR; i++) flash[s + i] = (uint8_t)rnd();
        op();
        memset(&flash[s], 0xFF, SECTOR);
    }
}

static void sim_program(uint32_t off, const uint8_t *d, uint32_t len) {
    if (off % PAGE || len % PAGE || off + len > FLASH_BYTES) { puts("BAD PROGRAM"); exit(1); }
    for (uint32_t p = off; p < off + len; p += PAGE) {
        if (cut_at >= 0 && ops == cut_at)                 // torn: some bits cleared
            for (uint32_t i = 0; i < PAGE; i++) flash[p + i] &= (uint8_t)(d[p - off + i] | rnd());
        op();
        for (uint32_t i = 0; i < PAGE; i++) flash[p + i] &= d[p - off + i];
    }
}

static const fw_flash_t F = { flash, LINK, sim_erase, sim_program };

// ── What the board does at power-up ─────────────────────────────────────────
// The boot ROM's boot2 check: CRC-32 with poly 0x04C11DB7, init ~0, no
// reflection, no final XOR, over the first 252 bytes, against the last 4.
static uint32_t crc32_mpeg2(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= (uint32_t)*p++ << 24;
        for (int k = 0; k < 8; k++) c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : c << 1;
    }
    return c;
}

static int boot2_ok(const uint8_t *f) {
    uint32_t want = (uint32_t)f[252] | (uint32_t)f[253] << 8 | (uint32_t)f[254] << 16 | (uint32_t)f[255] << 24;
    return crc32_mpeg2(f, 252) == want;
}

enum { BOOT_OLD, BOOT_SLOT, BOOTSEL_ROM, BOOTSEL_STAGE0, HANG };
static const char *OUT_NAME[] = { "old bootloader", "slot", "BOOTSEL (boot2 CRC)",
                                  "BOOTSEL (stage 0: no valid slot)", "HANG" };

static uint8_t *legacy, *stage0, *slot_a, *slot_b, *usb;
static size_t legacy_len, stage0_len, slot_a_len, slot_b_len, usb_len;

static int power_up(void) {
    if (!boot2_ok(flash)) return BOOTSEL_ROM;
    if (memcmp(flash, legacy, legacy_len) == 0) return BOOT_OLD;
    if (memcmp(flash, stage0, stage0_len) == 0)
        return fw_slot_select(flash, LINK) >= 0 ? BOOT_SLOT : BOOTSEL_STAGE0;
    return HANG;                       // valid boot2 in front of something else
}

// ── Helpers ─────────────────────────────────────────────────────────────────
static uint8_t *load(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); *len = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*len);
    if (fread(b, 1, *len, f) != *len) exit(2);
    fclose(f);
    return b;
}

static fw_boot_images_t images(void) {
    fw_boot_images_t i = { stage0, (uint32_t)stage0_len, { slot_a, slot_b },
                           { (uint32_t)slot_a_len, (uint32_t)slot_b_len } };
    return i;
}

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); puts(""); failures++; } } while (0)

static void write_slot_raw(int s, const uint8_t *img, uint32_t len, uint32_t seq, uint32_t tag) {
    uint32_t off = fw_slot_offset(s);
    memset(&flash[off], 0xFF, FW_SLOT_SIZE);
    memcpy(&flash[off], img, len);
    fw_slot_hdr_t h = { FW_SLOT_MAGIC, seq, len, fw_crc32(img, len), tag, 0 };
    h.check = fw_slot_hdr_check(&h);
    memcpy(&flash[off + FW_SLOT_HDR_AT], &h, sizeof(h));
}

static int final_ok(void) {
    fw_boot_images_t img = images();
    uint32_t tag = fw_boot_tag(&img);
    if (memcmp(flash, stage0, stage0_len) != 0) return 0;
    for (int s = 0; s < 2; s++) {
        const fw_slot_hdr_t *h = fw_slot_hdr(flash, s);
        if (!fw_slot_ok(flash, LINK, s) || h->tag != tag ||
            memcmp(&flash[fw_slot_offset(s)], s ? slot_b : slot_a, h->len) != 0) return 0;
    }
    return 1;
}

// Every cut point from `initial`, with a few tear patterns each.
static void sweep(const char *name, const uint8_t *initial, int seeds) {
    fw_boot_images_t img = images();
    memcpy(flash, initial, FLASH_BYTES);
    ops = 0; cut_at = -1;
    int r = fw_boot_install(&F, &img);
    long total = ops;
    CHECK(r >= 0 && final_ok(), "%s: uninterrupted run returned %d", name, r);
    printf("%s: %ld flash operations, result %d\n", name, total, r);

    int count[5] = {0};
    for (long at = 0; at < total; at++) {
        for (int seed = 1; seed <= seeds; seed++) {
            rng = (unsigned)(at * 131 + seed);
            memcpy(flash, initial, FLASH_BYTES);
            ops = 0; cut_at = at;
            if (setjmp(cut) == 0) {
                fw_boot_install(&F, &img);
                CHECK(0, "%s: cut at op %ld never reached", name, at);
                continue;
            }
            int out = power_up();
            count[out]++;
            CHECK(out != HANG && out != BOOTSEL_STAGE0, "%s: cut at op %ld (seed %d): %s",
                  name, at, seed, OUT_NAME[out]);
            if (out == BOOTSEL_ROM)                     // recover over USB, as documented
                memcpy(flash, usb, usb_len);
            ops = 0; cut_at = -1;
            r = fw_boot_install(&F, &img);
            CHECK(r >= 0 && final_ok(), "%s: resume after cut at op %ld returned %d", name, at, r);
        }
    }
    printf("  after a cut: %d old bootloader, %d slot, %d BOOTSEL; never a hang\n",
           count[BOOT_OLD], count[BOOT_SLOT], count[BOOTSEL_ROM]);
}

int main(int argc, char **argv) {
    if (argc != 6) { puts("usage: test_boot_install STAGE0 SLOT_A SLOT_B LEGACY USB"); return 2; }
    stage0 = load(argv[1], &stage0_len);
    slot_a = load(argv[2], &slot_a_len);
    slot_b = load(argv[3], &slot_b_len);
    legacy = load(argv[4], &legacy_len);
    usb    = load(argv[5], &usb_len);
    static uint8_t initial[FLASH_BYTES];
    fw_boot_images_t img = images();
    uint32_t tag = fw_boot_tag(&img);

    // 0. The pieces the rest relies on.
    {
        static const uint8_t check[] = "123456789";
        CHECK(fw_crc32(check, 9) == 0xCBF43926u, "fw_crc32 is not the standard CRC-32");
        CHECK(boot2_ok(stage0) && boot2_ok(legacy), "boot2 CRC check rejects a real boot2");
        uint8_t bad[256]; memcpy(bad, stage0, 256); bad[100] ^= 1;
        CHECK(!boot2_ok(bad), "boot2 CRC check accepts a corrupted boot2");
        memset(flash, 0xFF, FLASH_BYTES);
        memcpy(flash, usb, usb_len);
        CHECK(power_up() == BOOT_SLOT && fw_slot_select(flash, LINK) == 0 &&
              fw_slot_hdr(flash, 0)->tag == tag, "USB image does not boot slot A with the tag");
    }

    // 1. Old board: single bootloader at 0, an app, slot B region blank.
    memset(initial, 0xFF, FLASH_BYTES);
    memcpy(initial, legacy, legacy_len);
    memset(&initial[FW_APP_OFFSET], 0x5A, 4096);
    sweep("migrate an old board", initial, 4);

    // 2. Migrated board holding an older build in both slots, B newer.
    uint8_t *old_a = malloc(slot_a_len), *old_b = malloc(slot_b_len);
    memcpy(old_a, slot_a, slot_a_len); old_a[slot_a_len - 1] ^= 0xFF;
    memcpy(old_b, slot_b, slot_b_len); old_b[slot_b_len - 1] ^= 0xFF;
    memset(initial, 0xFF, FLASH_BYTES);
    memcpy(initial, stage0, stage0_len);
    memcpy(flash, initial, FLASH_BYTES);
    write_slot_raw(0, old_a, (uint32_t)slot_a_len, 4, 0x11111111u);
    write_slot_raw(1, old_b, (uint32_t)slot_b_len, 5, 0x11111111u);
    memcpy(initial, flash, FLASH_BYTES);
    sweep("update a migrated board", initial, 4);

    // 3. Up to date: no flash operations at all.
    memcpy(flash, initial, FLASH_BYTES);
    ops = 0; cut_at = -1;
    fw_boot_install(&F, &img);
    long before = ops;
    int r = fw_boot_install(&F, &img);
    CHECK(r == FW_BI_UP_TO_DATE && ops == before, "up-to-date board: result %d, %ld writes",
          r, ops - before);

    // 4. Sector 0 holds a different stage 0: refused, nothing written.
    memcpy(flash, initial, FLASH_BYTES);
    flash[stage0_len - 1] ^= 0xFF;
    memcpy(initial, flash, FLASH_BYTES);
    ops = 0;
    r = fw_boot_install(&F, &img);
    CHECK(r == FW_BI_E_STAGE0 && ops == 0 && memcmp(flash, initial, FLASH_BYTES) == 0,
          "foreign stage 0: result %d, %ld writes", r, ops);

    printf(failures ? "\n%d FAILURE(S)\n" : "\nall passed\n", failures);
    return failures != 0;
}
