// Stage 0 — the code in flash sector 0, behind boot2. Written once, never again.
//
// boot2 sets up XIP and enters here through the vector table at 0x10000100,
// exactly as it would enter an ordinary image. Stage 0 picks a bootloader slot
// (common/boot_slots.h) and enters it the same way. If neither slot is valid
// it hands the board to the boot ROM's USB BOOTSEL, so it stays recoverable
// over USB. Design: docs/FIELD_UPDATE.md → "Updating the bootloader over the bus".
//
// No runtime: no .data, no .bss, no libc, clocks as boot2 left them. The stack
// is the top of SRAM, which the bootloader's own start-up then re-sets.

#include <stdint.h>
#include "boot_slots.h"

#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08u)

void __attribute__((noreturn)) stage0_main(void);

static const struct {
    uint32_t sp;
    void (*reset)(void);
    uint32_t mark;             // FW_STAGE0_MARK_AT
} vectors __attribute__((section(".vectors"), used)) = {
    0x20042000u, stage0_main, FW_STAGE0_MAGIC,
};

typedef void *(*rom_lookup_fn)(const uint16_t *table, uint32_t code);
typedef void (*rom_usb_boot_fn)(uint32_t gpio_activity_mask, uint32_t disable_mask);

// Boot ROM function lookup: 16-bit pointers at 0x14 (function table) and 0x18
// (lookup routine), per the RP2040 datasheet's bootrom section.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static void __attribute__((noreturn)) usb_boot(void) {
    const uint16_t *table = (const uint16_t *)(uintptr_t)*(const volatile uint16_t *)0x14u;
    rom_lookup_fn lookup = (rom_lookup_fn)(uintptr_t)*(const volatile uint16_t *)0x18u;
    rom_usb_boot_fn f = (rom_usb_boot_fn)lookup(table, 'U' | ('B' << 8));
    f(0, 0);
    while (1) {}
}
#pragma GCC diagnostic pop

void __attribute__((noreturn)) stage0_main(void) {
    const uint8_t *flash = (const uint8_t *)FW_XIP_LINK_BASE;
    int slot = fw_slot_select(flash, FW_XIP_LINK_BASE);
    if (slot < 0) usb_boot();

    const uint32_t *vt = (const uint32_t *)(FW_XIP_LINK_BASE + fw_slot_offset(slot) + 0x100u);
    SCB_VTOR = (uint32_t)vt;
    __asm volatile ("msr msp, %0\n"
                    "bx %1\n"
                    : : "r"(vt[0]), "r"(vt[1]));
    __builtin_unreachable();
}
