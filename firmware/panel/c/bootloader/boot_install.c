// Ordering is the whole point of this file. The rules:
//   - A slot header is written only after its image verifies, so a slot cut off
//     mid-write has no valid header and stage 0 skips it.
//   - The slot stage 0 is booting is never written; only the other one.
//   - Sector 0 is written once, while migrating an old board, and only after a
//     slot is valid. Its boot2 page goes last, so until stage 0 is complete the
//     boot ROM's boot2 CRC check fails and the board falls back to USB BOOTSEL.

#include <string.h>
#include "boot_slots.h"
#include "boot_install.h"

#define PAGE   256u
#define TRIES  3

static uint8_t page_buf[PAGE];

static void program_from(const fw_flash_t *f, uint32_t off, const uint8_t *src,
                         uint32_t len, uint32_t first_page, uint32_t end_page) {
    for (uint32_t p = first_page; p < end_page; p++) {
        uint32_t at = p * PAGE;
        memset(page_buf, 0xFF, PAGE);
        if (at < len) memcpy(page_buf, src + at, (len - at) < PAGE ? (len - at) : PAGE);
        f->program(off + at, page_buf, PAGE);
    }
}

static bool image_usable(const uint8_t *img, uint32_t len, uint32_t max) {
    return img && len >= 0x108u && len <= max;
}

uint32_t fw_boot_tag(const fw_boot_images_t *img) {
    return fw_crc32_from(fw_crc32(img->slot[0], img->slot_len[0]), img->slot[1], img->slot_len[1]);
}

static bool slot_holds(const fw_flash_t *f, int s, const fw_boot_images_t *img) {
    const fw_slot_hdr_t *h = fw_slot_hdr(f->flash, s);
    return fw_slot_ok(f->flash, f->link_base, s) &&
           h->len == img->slot_len[s] &&
           h->crc == fw_crc32(img->slot[s], img->slot_len[s]) &&
           h->tag == fw_boot_tag(img);
}

static bool write_slot(const fw_flash_t *f, int s, const fw_boot_images_t *img, uint32_t seq) {
    uint32_t off = fw_slot_offset(s), len = img->slot_len[s];
    fw_slot_hdr_t h = {
        .magic = FW_SLOT_MAGIC, .seq = seq, .len = len,
        .crc = fw_crc32(img->slot[s], len), .tag = fw_boot_tag(img),
    };
    h.check = fw_slot_hdr_check(&h);

    for (int t = 0; t < TRIES; t++) {
        f->erase(off, FW_SLOT_SIZE);                     // the header goes first
        program_from(f, off, img->slot[s], len, 0, (len + PAGE - 1u) / PAGE);
        if (memcmp(f->flash + off, img->slot[s], len) != 0) continue;

        memset(page_buf, 0xFF, PAGE);
        memcpy(page_buf, &h, sizeof(h));
        f->program(off + FW_SLOT_HDR_AT, page_buf, PAGE);
        if (fw_slot_ok(f->flash, f->link_base, s)) return true;
    }
    return false;
}

static bool write_stage0(const fw_flash_t *f, const fw_boot_images_t *img) {
    uint32_t pages = FW_STAGE0_SIZE / PAGE;
    for (int t = 0; t < TRIES; t++) {
        f->erase(FW_STAGE0_OFFSET, FW_STAGE0_SIZE);      // boot2 gone: BOOTSEL from here
        program_from(f, FW_STAGE0_OFFSET, img->stage0, img->stage0_len, 1, pages);
        if (memcmp(f->flash + PAGE, img->stage0 + PAGE, img->stage0_len - PAGE) != 0) continue;
        program_from(f, FW_STAGE0_OFFSET, img->stage0, img->stage0_len, 0, 1);   // boot2 last
        if (memcmp(f->flash, img->stage0, img->stage0_len) == 0) return true;
    }
    return false;
}

int fw_boot_install(const fw_flash_t *f, const fw_boot_images_t *img) {
    if (!image_usable(img->stage0, img->stage0_len, FW_STAGE0_SIZE) ||
        !image_usable(img->slot[0], img->slot_len[0], FW_SLOT_IMAGE_MAX) ||
        !image_usable(img->slot[1], img->slot_len[1], FW_SLOT_IMAGE_MAX) ||
        !fw_stage0_present(img->stage0))
        return FW_BI_E_IMAGE;

    int result = FW_BI_UP_TO_DATE;

    if (fw_stage0_present(f->flash)) {
        if (memcmp(f->flash, img->stage0, img->stage0_len) != 0) return FW_BI_E_STAGE0;
    } else {
        // Old single bootloader at 0x000000, which overlaps slot A. Slot B is
        // free: fill it while the old bootloader still boots, then stage 0.
        if (!write_slot(f, 1, img, 1)) return FW_BI_E_VERIFY;
        if (!write_stage0(f, img)) return FW_BI_E_VERIFY;
        result = FW_BI_MIGRATED;
    }

    // Write the slot stage 0 is not booting until both hold this build. At
    // most two passes: the other slot, then (now that it boots) the first.
    for (int pass = 0; pass < 3; pass++) {
        int sel = fw_slot_select(f->flash, f->link_base);
        if (sel < 0) return FW_BI_E_VERIFY;          // cannot happen after a valid write
        int other = !sel;
        if (slot_holds(f, sel, img) && slot_holds(f, other, img)) return result;
        if (!write_slot(f, other, img, fw_slot_hdr(f->flash, sel)->seq + 1u)) return FW_BI_E_VERIFY;
        if (result == FW_BI_UP_TO_DATE) result = FW_BI_INSTALLED;
    }
    return FW_BI_E_VERIFY;
}
