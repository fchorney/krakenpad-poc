// Writes stage 0 and the bootloader slots in an order that survives a power
// cut at any point (docs/FIELD_UPDATE.md → "Updating the bootloader over the
// bus"). Runs inside the boot updater app on the panel, and inside
// host_test/test_boot_install.c against a simulated flash.
#ifndef BOOT_INSTALL_H
#define BOOT_INSTALL_H

#include <stdint.h>

typedef struct {
    const uint8_t *flash;      // read view of offset 0
    uint32_t link_base;        // where code runs from it (FW_XIP_LINK_BASE)
    void (*erase)(uint32_t off, uint32_t len);                        // whole sectors
    void (*program)(uint32_t off, const uint8_t *data, uint32_t len); // whole pages
} fw_flash_t;

typedef struct {
    const uint8_t *stage0;     // stage 0 image, at most FW_STAGE0_SIZE
    uint32_t stage0_len;
    const uint8_t *slot[2];    // the bootloader built for slot A and for slot B
    uint32_t slot_len[2];
} fw_boot_images_t;

enum {
    FW_BI_UP_TO_DATE = 0,      // both slots already held this build
    FW_BI_INSTALLED  = 1,      // wrote at least one slot; both now hold it
    FW_BI_MIGRATED   = 2,      // old single bootloader replaced by stage 0 + slots
    FW_BI_E_STAGE0   = -1,     // sector 0 holds a different stage 0: not touched
    FW_BI_E_VERIFY   = -2,     // a write did not verify after retries
    FW_BI_E_IMAGE    = -3,     // an embedded image is unusable (size, vectors)
};

// The tag every slot header carries: CRC-32 of the slot A build followed by the
// slot B build, so a change to either one changes it.
uint32_t fw_boot_tag(const fw_boot_images_t *img);

int fw_boot_install(const fw_flash_t *f, const fw_boot_images_t *img);

#endif
