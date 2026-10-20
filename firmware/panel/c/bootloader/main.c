// Panel bootloader — dual-panel rev 1.
//
// Receives new application images over the RS-485 bus. Design and protocol:
// docs/FIELD_UPDATE.md. Flash map and command letters: ../common/fw_layout.h.
//
// Built twice, once per bootloader slot (FW_SLOT 0 = A at 0x10001000, 1 = B at
// 0x10200000); stage0.c picks which one runs. Without FW_SLOT (the host test)
// it is the old single bootloader.
//
// Boot sequence:
//   1. If the boot-control record says an install was interrupted, redo it.
//   2. Hold (stay resident) if the app asked for it via watchdog scratch, or
//      if the app region does not look runnable.
//   3. Otherwise listen 200 ms for 'B'. Heard -> hold. Silence -> run the app.
//
// While holding it answers 'Q' and takes 'U' / 'D' / 'A' / 'X'. The whole
// binary is copy_to_ram, so erasing and programming flash cannot stall it:
// the UART IRQ keeps filling a RAM ring throughout.
//
// Three rules carried over from the bring-up responder (see its comments):
// never answer a broadcast, release DE on BUSY not FIFO-empty, and always drain
// the RX FIFO in the ISR. One rule is new: after any operation long enough that
// the master has given up waiting (erase, CRC, copy), DISCARD everything that
// arrived meanwhile, so a stale 'Q' is never answered late on top of later
// traffic. The host retries.

#include <stddef.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/nvic.h"
#include "hardware/structs/systick.h"
#include "fw_layout.h"
#include "boot_slots.h"

// ── As-built pins (docs/DUAL_PANEL.md) ──────────────────────────────────────
#define PIN_RS485_TX    0
#define PIN_RS485_RX    1
#define PIN_RS485_DE    4    // DE + ~RE tied; HIGH = transmit
#define PIN_DEBUG_LED   16   // carrier D202 — the only thing that shows we are here
#define PIN_DIP_ID3     18   // GPIO18 is bit 3, GPIO21 bit 0; closed reads 0
#define PIN_DIP_ID2     19
#define PIN_DIP_ID1     20
#define PIN_DIP_ID0     21
#define PIN_INT_OUT     22   // must stay truly hi-Z: the pad resets with a pull-down

#define RS485_BAUD      1000000u
#define RS485_SYNC      0x55
#define RS485_BCAST     0xFF
#define RS485_MAX_PAY   80

#define BOOT_LISTEN_US  200000u
// A gap this long mid-packet abandons the packet, as in the gameplay firmware
// (docs/RS485_PROTOCOL.md → "Framing"): longer than the 32-byte RX FIFO takes
// to fill, shorter than any gap the master leaves between packets.
#define RS485_GAP_US    500u
#define RING_GAP_FLAG   0x100u           // ring entry: idle gap before this byte

// ── RX ring ─────────────────────────────────────────────────────────────────
// 32 KB is ~330 ms of a saturated 1 Mbps bus. Nothing the bootloader does
// while frames it needs are arriving takes longer than one page program
// (3 ms worst case); the long operations discard the ring afterwards anyway.
#define RING_SIZE 32768u
static volatile uint16_t ring[RING_SIZE];
static volatile uint32_t ring_head = 0, ring_tail = 0;
static volatile bool     ring_resync = false;
static uint32_t          ring_last_rx_us = 0;   // ISR only

static void rx_irq(void) {
    // Timed per IRQ, not per byte: one drain of a contiguous stream is never
    // more than a FIFO-fill apart from the last, well under RS485_GAP_US.
    uint32_t now = time_us_32();
    uint16_t flag = (now - ring_last_rx_us) > RS485_GAP_US ? RING_GAP_FLAG : 0;
    ring_last_rx_us = now;
    while (uart_is_readable(uart0)) {
        uint32_t dr = uart_get_hw(uart0)->dr;
        if (dr & 0xF00u) ring_resync = true;          // FE | PE | BE | OE
        uint32_t next = (ring_head + 1u) % RING_SIZE;
        if (next == ring_tail) continue;              // drop the byte, never the drain
        ring[ring_head] = (uint16_t)((dr & 0xFFu) | flag);
        ring_head = next;
        flag = 0;
    }
}

static void discard_rx(void) {
    ring_tail = ring_head;
    ring_resync = true;
}

// ── Helpers ─────────────────────────────────────────────────────────────────
static uint32_t crc_table[256];

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
        crc_table[i] = c;
    }
}

// Standard CRC-32 (zlib / Python's zlib.crc32), so the host computes the same.
static uint32_t crc32_buf(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) c = crc_table[(c ^ *p++) & 0xFFu] ^ (c >> 8);
    return ~c;
}

static uint8_t crc8_update(uint8_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    return crc;
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_u16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static const uint8_t *flash_ptr(uint32_t offset) {
    return (const uint8_t *)(XIP_BASE + offset);
}

static uint32_t round_up_sector(uint32_t n) {
    return (n + FLASH_SECTOR_SIZE - 1u) & ~(FLASH_SECTOR_SIZE - 1u);
}

static uint8_t dip_read(void) {
    return (uint8_t)((gpio_get(PIN_DIP_ID3) << 3) | (gpio_get(PIN_DIP_ID2) << 2) |
                     (gpio_get(PIN_DIP_ID1) << 1) |  gpio_get(PIN_DIP_ID0));
}

static uint32_t g_boot_tag;

// From this slot's header: the CRC-32 of the slot A and slot B builds, the
// same in both slots, so the host can check it against the files it sent. The host test
// build has no slot and keeps the old FNV-1a of the build timestamp.
static void compute_boot_tag(void) {
#ifdef FW_SLOT
    const fw_slot_hdr_t *h = fw_slot_hdr(flash_ptr(0), FW_SLOT);
    g_boot_tag = fw_slot_hdr_ok(h) ? h->tag : 0;
#else
    const char *s = __DATE__ " " __TIME__;
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    g_boot_tag = h;
#endif
}

// ── Boot-control record ─────────────────────────────────────────────────────
static fw_ctrl_t g_ctrl;

static uint32_t ctrl_check(const fw_ctrl_t *c) {
    return crc32_buf((const uint8_t *)c, offsetof(fw_ctrl_t, check));
}

static void ctrl_load(void) {
    memcpy(&g_ctrl, flash_ptr(FW_CTRL_OFFSET), sizeof(g_ctrl));
    if (g_ctrl.magic != FW_CTRL_MAGIC || g_ctrl.check != ctrl_check(&g_ctrl)) {
        memset(&g_ctrl, 0, sizeof(g_ctrl));      // no record: app came over USB/SWD
        g_ctrl.magic = FW_CTRL_MAGIC;
    }
}

static void ctrl_store(void) {
    static uint8_t page[FLASH_PAGE_SIZE];
    g_ctrl.magic = FW_CTRL_MAGIC;
    g_ctrl.check = ctrl_check(&g_ctrl);
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &g_ctrl, sizeof(g_ctrl));
    flash_range_erase(FW_CTRL_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FW_CTRL_OFFSET, page, sizeof(page));
}

// ── Application checks and hand-off ─────────────────────────────────────────
static bool app_vectors_sane(void) {
    const uint32_t *vt = (const uint32_t *)flash_ptr(FW_APP_OFFSET + FW_APP_VTOR_OFFSET);
    uint32_t sp = vt[0], pc = vt[1];
    uint32_t lo = FW_XIP_LINK_BASE + FW_APP_OFFSET + FW_APP_VTOR_OFFSET;
    uint32_t hi = FW_XIP_LINK_BASE + FW_APP_OFFSET + FW_APP_SIZE;
    return sp >= 0x20000000u && sp <= 0x20042000u && (sp & 3u) == 0 &&
           (pc & 1u) && pc >= lo && pc < hi;
}

// Runnable = the vector table points somewhere plausible, and if this
// bootloader installed the app, its CRC still matches.
static bool app_valid(void) {
    if (!app_vectors_sane()) return false;
    if (g_ctrl.app_len == 0) return true;
    if (g_ctrl.app_len > FW_APP_SIZE) return false;
    return crc32_buf(flash_ptr(FW_APP_OFFSET), g_ctrl.app_len) == g_ctrl.app_crc;
}

#ifdef BOOTLOADER_HOST_TEST
void __attribute__((noreturn)) host_run_app(void);   // host_test/: record it, exit
#endif

static void __attribute__((noreturn)) run_app(void) {
#ifdef BOOTLOADER_HOST_TEST
    host_run_app();
#else
    gpio_put(PIN_DEBUG_LED, 0);
    gpio_put(PIN_RS485_DE, 0);
    uart_set_irq_enables(uart0, false, false);
    irq_set_enabled(UART0_IRQ, false);
    uart_deinit(uart0);

    // Leave the core as boot2 would: no interrupts live or pending, SysTick off.
    // The application's runtime_init resets the peripherals and clocks itself.
    systick_hw->csr = 0;
    nvic_hw->icer = 0xFFFFFFFFu;
    nvic_hw->icpr = 0xFFFFFFFFu;

    const uint32_t *vt = (const uint32_t *)flash_ptr(FW_APP_OFFSET + FW_APP_VTOR_OFFSET);
    scb_hw->vtor = (uint32_t)vt;
    __asm volatile ("msr msp, %0\n"
                    "bx %1\n"
                    : : "r"(vt[0]), "r"(vt[1]));
    __builtin_unreachable();
#endif
}

// ── Bus ─────────────────────────────────────────────────────────────────────
static void bus_send(uint8_t cmd, uint8_t addr, const uint8_t *pay, uint8_t len) {
    uint8_t hdr[4] = { RS485_SYNC, cmd, addr, len };
    uint8_t crc = 0;
    crc = crc8_update(crc, cmd);
    crc = crc8_update(crc, addr);
    crc = crc8_update(crc, len);
    for (uint8_t i = 0; i < len; i++) crc = crc8_update(crc, pay[i]);

    gpio_put(PIN_RS485_DE, 1);
    busy_wait_us(2);
    uart_write_blocking(uart0, hdr, 4);
    if (len) uart_write_blocking(uart0, pay, len);
    uart_write_blocking(uart0, &crc, 1);
    uart_tx_wait_blocking(uart0);         // BUSY flag: the shift register is empty
    busy_wait_us(12);
    gpio_put(PIN_RS485_DE, 0);
}

static void bus_init(void) {
    uart_init(uart0, RS485_BAUD);
    gpio_set_function(PIN_RS485_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RS485_RX, GPIO_FUNC_UART);
    gpio_pull_up(PIN_RS485_RX);           // idle-mark while our own DE blinds RO
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    uart_set_hw_flow(uart0, false, false);
    while (uart_is_readable(uart0)) (void)uart_get_hw(uart0)->dr;
    ring_last_rx_us = time_us_32();
    irq_set_exclusive_handler(UART0_IRQ, rx_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
}

// ── Update state ────────────────────────────────────────────────────────────
static uint8_t  g_state = FW_ST_IDLE;
static bool     g_hold  = false;
static uint32_t up_len, up_crc, up_blocks, up_received;
static uint8_t  up_bitmap[FW_MAX_BLOCKS / 8];

// Blocks are 64 bytes; flash programs 256-byte pages. Accumulate a page and
// program it when it is complete or the stream moves to another page. A page
// programmed with holes stays 0xFF in the holes, so a gap block re-sent later
// can be programmed into the same page: NOR programming only clears bits.
static int32_t  page_idx = -1;
static uint8_t  page_buf[FLASH_PAGE_SIZE];
static uint8_t  page_have;

static void page_flush(void) {
    if (page_idx < 0) return;
    flash_range_program(FW_STAGE_OFFSET + (uint32_t)page_idx * FLASH_PAGE_SIZE,
                        page_buf, FLASH_PAGE_SIZE);
    page_idx = -1;
    page_have = 0;
}

static uint8_t page_need(int32_t p) {
    uint32_t first = (uint32_t)p * 4u, last = first + 3u;
    if (last >= up_blocks) last = up_blocks - 1u;
    return (uint8_t)((1u << (last - first + 1u)) - 1u);
}

static void do_begin(uint32_t len, uint32_t crc) {
    page_idx = -1;
    page_have = 0;
    if (len == 0 || len > FW_APP_SIZE) { g_state = FW_ST_REFUSED; return; }
    g_state = FW_ST_ERASING;
    gpio_put(PIN_DEBUG_LED, 1);
    flash_range_erase(FW_STAGE_OFFSET, round_up_sector(len));
    up_len = len;
    up_crc = crc;
    up_blocks = (len + FW_BLOCK_BYTES - 1u) / FW_BLOCK_BYTES;
    up_received = 0;
    memset(up_bitmap, 0, sizeof(up_bitmap));
    g_state = FW_ST_RECEIVING;
    discard_rx();
}

static void do_data(uint32_t idx, const uint8_t *data, uint32_t n) {
    if (g_state != FW_ST_RECEIVING || idx >= up_blocks) return;
    uint32_t want = (idx == up_blocks - 1u) ? up_len - idx * FW_BLOCK_BYTES : FW_BLOCK_BYTES;
    if (n != want) return;
    if (up_bitmap[idx >> 3] & (1u << (idx & 7u))) return;      // duplicate

    int32_t p = (int32_t)(idx / 4u);
    if (p != page_idx) {
        page_flush();
        page_idx = p;
        memset(page_buf, 0xFF, sizeof(page_buf));
    }
    memcpy(&page_buf[(idx % 4u) * FW_BLOCK_BYTES], data, n);
    page_have |= (uint8_t)(1u << (idx % 4u));
    up_bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
    up_received++;
    if (page_have == page_need(p)) page_flush();

    if (up_received == up_blocks) {
        page_flush();
        uint32_t c = crc32_buf(flash_ptr(FW_STAGE_OFFSET), up_len);
        g_state = (c == up_crc) ? FW_ST_STAGED : FW_ST_CRC_FAIL;
        discard_rx();
    }
}

// Staging -> application, then verify. Used by 'A' and by the boot-time
// resume of an interrupted install. The record is marked pending BEFORE the
// app region is touched, so a power cut anywhere in here redoes the copy.
static bool install(uint32_t len, uint32_t crc) {
    static uint8_t buf[FLASH_SECTOR_SIZE];
    gpio_put(PIN_DEBUG_LED, 1);
    g_ctrl.pending = 1;
    g_ctrl.staged_len = len;
    g_ctrl.staged_crc = crc;
    ctrl_store();

    uint32_t span = round_up_sector(len);
    flash_range_erase(FW_APP_OFFSET, span);
    for (uint32_t off = 0; off < span; off += FLASH_SECTOR_SIZE) {
        memcpy(buf, flash_ptr(FW_STAGE_OFFSET + off), FLASH_SECTOR_SIZE);
        flash_range_program(FW_APP_OFFSET + off, buf, FLASH_SECTOR_SIZE);
    }
    if (crc32_buf(flash_ptr(FW_APP_OFFSET), len) != crc) return false;

    g_ctrl.pending = 0;
    g_ctrl.app_len = len;
    g_ctrl.app_crc = crc;
    ctrl_store();
    return true;
}

static void do_activate(uint32_t crc) {
    if (g_state != FW_ST_STAGED || crc != up_crc) return;
    g_state = install(up_len, up_crc) ? FW_ST_INSTALLED : FW_ST_COPY_FAIL;
    discard_rx();
}

static void do_exit(void) {
    g_state = FW_ST_IDLE;
    if (app_valid()) run_app();
    // Nothing runnable: stay resident, still reachable.
}

static void reply_query(uint8_t me) {
    uint8_t r[16 + 2 * 16];
    uint32_t missing = 0;
    uint8_t n_list = 0;
    if (g_state == FW_ST_RECEIVING) {
        missing = up_blocks - up_received;
        for (uint32_t i = 0; i < up_blocks && n_list < 16; i++)
            if (!(up_bitmap[i >> 3] & (1u << (i & 7u)))) put_u16(&r[16 + 2 * n_list++], i);
    }
    r[0] = g_state;
#ifdef FW_SLOT
    const uint8_t from_b = FW_SLOT ? 4u : 0;
#else
    const uint8_t from_b = 0;
#endif
    r[1] = (uint8_t)((app_vectors_sane() ? 1u : 0) | (g_ctrl.pending ? 2u : 0) | from_b);
    put_u16(&r[2], missing > 0xFFFF ? 0xFFFF : missing);
    put_u32(&r[4], g_ctrl.app_len);
    put_u32(&r[8], g_ctrl.app_crc);
    put_u32(&r[12], g_boot_tag);
    bus_send(FW_CMD_QUERY_ACK, me, r, (uint8_t)(16 + 2 * n_list));
}

static void handle(uint8_t cmd, uint8_t addr, const uint8_t *pay, uint8_t len) {
    uint8_t me = dip_read();
    bool bcast = (addr == RS485_BCAST);
    if (addr != me && !bcast) return;

    if (cmd == FW_CMD_HOLD) {
        g_hold = true;
        if (!bcast) bus_send(FW_CMD_HOLD_ACK, me, NULL, 0);
        return;
    }
    if (!g_hold) return;                  // during the boot window only 'B' counts

    switch (cmd) {
    case FW_CMD_QUERY:
        if (!bcast) reply_query(me);
        break;
    case FW_CMD_BEGIN:
        if (len == 8) do_begin(get_u32(&pay[0]), get_u32(&pay[4]));
        break;
    case FW_CMD_DATA:
        if (len >= 3) do_data((uint32_t)pay[0] | ((uint32_t)pay[1] << 8), &pay[2], len - 2u);
        break;
    case FW_CMD_ACTIVATE:
        if (len == 4) do_activate(get_u32(&pay[0]));
        break;
    case FW_CMD_EXIT:
        do_exit();
        break;
    default:
        break;
    }
}

static void bus_service(void) {
    static enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
    static uint8_t cmd, addr, len, idx, crc, pay[RS485_MAX_PAY];

    if (ring_resync) { ring_resync = false; st = W_SYNC; }
    while (ring_tail != ring_head) {
        uint16_t e = ring[ring_tail];
        uint8_t  c = (uint8_t)e;
        ring_tail = (ring_tail + 1u) % RING_SIZE;
        if ((e & RING_GAP_FLAG) && st != W_SYNC) st = W_SYNC;   // packet cut off
        switch (st) {
        case W_SYNC: if (c == RS485_SYNC) { crc = 0; st = W_CMD; } break;
        case W_CMD:  cmd  = c; crc = crc8_update(crc, c); st = W_ADDR; break;
        case W_ADDR: addr = c; crc = crc8_update(crc, c); st = W_LEN;  break;
        case W_LEN:
            len = c; crc = crc8_update(crc, c); idx = 0;
            st = (len > RS485_MAX_PAY) ? W_SYNC : (len ? W_PAY : W_CRC);
            break;
        case W_PAY:
            pay[idx++] = c; crc = crc8_update(crc, c);
            if (idx >= len) st = W_CRC;
            break;
        case W_CRC:
            st = W_SYNC;
            if (c == crc) {
                handle(cmd, addr, pay, len);
                // A long operation discards the ring; stop walking the old view.
                if (ring_resync) return;
            }
            break;
        }
    }
}

// Debug LED: 2 Hz idle, 8 Hz receiving, solid while staged / installed, and a
// fast 16 Hz flicker for any failure. Erase and copy hold it solid on.
static void led_update(void) {
    uint32_t t = to_ms_since_boot(get_absolute_time());
    bool on;
    switch (g_state) {
    case FW_ST_RECEIVING:  on = (t / 62u) & 1u;  break;
    case FW_ST_STAGED:
    case FW_ST_INSTALLED:  on = true;            break;
    case FW_ST_CRC_FAIL:
    case FW_ST_REFUSED:
    case FW_ST_COPY_FAIL:  on = (t / 31u) & 1u;  break;
    default:               on = (t / 250u) & 1u; break;
    }
    gpio_put(PIN_DEBUG_LED, on);
}

int main(void) {
    gpio_init(PIN_RS485_DE);
    gpio_set_dir(PIN_RS485_DE, GPIO_OUT);
    gpio_put(PIN_RS485_DE, 0);            // receive, before anything else

    gpio_init(PIN_INT_OUT);
    gpio_disable_pulls(PIN_INT_OUT);      // hi-Z for real, or the master sees 2.7 V
    gpio_set_dir(PIN_INT_OUT, GPIO_IN);

    gpio_init(PIN_DEBUG_LED);
    gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);
    gpio_put(PIN_DEBUG_LED, 0);

    const uint dip[] = {PIN_DIP_ID0, PIN_DIP_ID1, PIN_DIP_ID2, PIN_DIP_ID3};
    for (uint i = 0; i < count_of(dip); i++) {
        gpio_init(dip[i]);
        gpio_set_dir(dip[i], GPIO_IN);
        gpio_pull_up(dip[i]);
    }

    crc32_init();
    compute_boot_tag();
    ctrl_load();

    bool hold_requested = (watchdog_hw->scratch[0] == FW_HOLD_MAGIC);
    watchdog_hw->scratch[0] = 0;

    // Interrupted install: staging is never written during a copy, so if it
    // still verifies, redo the copy. If it does not, leave things as they are;
    // app_valid() below decides whether what is in the app region can run.
    if (g_ctrl.pending && g_ctrl.staged_len && g_ctrl.staged_len <= FW_APP_SIZE &&
        crc32_buf(flash_ptr(FW_STAGE_OFFSET), g_ctrl.staged_len) == g_ctrl.staged_crc) {
        install(g_ctrl.staged_len, g_ctrl.staged_crc);
    }

    bus_init();

    g_hold = hold_requested || !app_valid();
    if (!g_hold) {
        uint64_t until = time_us_64() + BOOT_LISTEN_US;
        while (time_us_64() < until && !g_hold) bus_service();
        if (!g_hold) run_app();
    }

    while (true) {
        bus_service();
        led_update();
    }
}
