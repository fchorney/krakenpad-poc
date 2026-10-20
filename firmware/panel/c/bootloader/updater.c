// Boot updater — an application that installs a new bootloader.
//
// Sent with the ordinary bus update (tools/panel_boot_update.py does it all).
// It carries stage 0 and the bootloader built for both slots, writes them with
// boot_install.c, then reboots into the bootloader with the hold flag set. The
// host reads the result back with 'Q': the bootloader tag must be the new
// build's. A panel left running this app answers nothing on the bus, but every
// boot ends in the held bootloader, so it never needs a cable to get back.
//
// Design: docs/FIELD_UPDATE.md → "Updating the bootloader over the bus".

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "fw_layout.h"
#include "boot_slots.h"
#include "boot_install.h"

#define PIN_RS485_DE    4
#define PIN_DEBUG_LED   16
#define PIN_INT_OUT     22

// updater_images.S
extern const uint8_t img_stage0[], img_stage0_end[];
extern const uint8_t img_slot_a[], img_slot_a_end[];
extern const uint8_t img_slot_b[], img_slot_b_end[];

static void do_erase(uint32_t off, uint32_t len) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(off, len);
    restore_interrupts(ints);
}

static void do_program(uint32_t off, const uint8_t *data, uint32_t len) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(off, data, len);
    restore_interrupts(ints);
}

#ifdef UPDATER_TEST_FALLBACK
// Bench test build (panel_boot_test_fallback), not for use otherwise: erases
// slot A's header, so stage 0 has to fall back to slot B on the next boot. It
// refuses unless stage 0 is in place and slot B is valid, so it cannot leave a
// board without a bootloader. The normal updater writes slot A back.
static int test_fallback(const fw_flash_t *f) {
    if (!fw_stage0_present(f->flash) || !fw_slot_ok(f->flash, f->link_base, 1)) return FW_BI_E_IMAGE;
    f->erase((FW_SLOT_A_OFFSET + FW_SLOT_HDR_AT) & ~(FLASH_SECTOR_SIZE - 1u), FLASH_SECTOR_SIZE);
    return FW_BI_INSTALLED;
}
#endif

static const char *result_name(int r) {
    switch (r) {
    case FW_BI_UP_TO_DATE: return "already up to date";
    case FW_BI_INSTALLED:  return "installed";
    case FW_BI_MIGRATED:   return "migrated from the single-bootloader layout";
    case FW_BI_E_STAGE0:   return "REFUSED: sector 0 holds a different stage 0";
    case FW_BI_E_VERIFY:   return "FAILED: a write did not verify";
    case FW_BI_E_IMAGE:    return "FAILED: an embedded image is unusable";
    default:               return "?";
    }
}

int main(void) {
    gpio_init(PIN_RS485_DE);
    gpio_set_dir(PIN_RS485_DE, GPIO_OUT);
    gpio_put(PIN_RS485_DE, 0);                // never drive the bus
    gpio_init(PIN_INT_OUT);
    gpio_disable_pulls(PIN_INT_OUT);          // hi-Z for real
    gpio_set_dir(PIN_INT_OUT, GPIO_IN);
    gpio_init(PIN_DEBUG_LED);
    gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);
    gpio_put(PIN_DEBUG_LED, 1);

    stdio_init_all();

    fw_boot_images_t img = {
        .stage0 = img_stage0, .stage0_len = (uint32_t)(img_stage0_end - img_stage0),
        .slot = { img_slot_a, img_slot_b },
        .slot_len = { (uint32_t)(img_slot_a_end - img_slot_a),
                      (uint32_t)(img_slot_b_end - img_slot_b) },
    };
    fw_flash_t f = {
        .flash = (const uint8_t *)XIP_BASE, .link_base = FW_XIP_LINK_BASE,
        .erase = do_erase, .program = do_program,
    };
#ifdef UPDATER_TEST_FALLBACK
    int r = test_fallback(&f);
#else
    int r = fw_boot_install(&f, &img);
#endif

    // Blink the result on D202 for 2 s: slow = done, fast = failed. The USB
    // console line is for the bench; nothing depends on it.
    uint32_t half = r >= 0 ? 250u : 50u;
    for (uint32_t t = 0; t < 2000u; t += half) {
        if (t == 1000u)
            printf("boot updater: %s (tag %08lX)\n", result_name(r),
                   (unsigned long)fw_boot_tag(&img));
        gpio_put(PIN_DEBUG_LED, (t / half) & 1u);
        sleep_ms(half);
    }

    watchdog_hw->scratch[0] = FW_HOLD_MAGIC;
    watchdog_reboot(0, 0, 10);
    while (true) tight_loop_contents();
}
