// Panel gameplay firmware — dual-panel rev 1 (carrier 2xx + brain 3xx).
//
// Built on the bring-up firmware's as-built HAL (bringup/main.c, which stays the
// bench tool). The breadboard version of this file used the prototype pin map
// and is in git history before 2026-10-18.
//
//   Core 0  FSR sampling loop: hysteresis + persistence, then a 2 ms panel
//           debounce both ways -> INT_OUT. Nothing else
//           runs here, so the press path never waits behind the bus or the LEDs.
//   Core 1  RS-485 responder, LED rendering (PIO fed by DMA, so a paint never
//           blocks a reply), SENSE_12V filter, USB console.
//
// Config (thresholds and channel flags) is RAM only by design: the panel boots
// on the defaults in firmware/shared/panel_config.h and runs whatever the
// master's last 'W' carried. The master persists it (docs/PANEL_CONFIG.md).
// Thresholds are calibrated units; core 1 tracks each sensor's tare and turns
// them into raw counts, so the press loop still compares raw values.
//
// LED modes (docs/PANEL_CONFIG.md → "LED modes"):
//   normal   paint the master's 'L' frames; dark if none for LED_FALLBACK_MS.
//            No valid bus frame at all for NO_MASTER_MS (no master): a slow
//            dim white breath, all white while the panel is pressed.
//   ID       white image of this panel's position from the DIP, X if not 0-8.
//            Shown for ID_BOOT_MS once 12V first arrives, and on master 'M'.
//   test     each edge lights its own colour, brightness tracking pressure.
//            DIP 10 runs it standalone; master 'M' runs it on bus panels.
//
// Runs behind panel_bootloader (linked at 0x10008000, docs/FIELD_UPDATE.md).
// As-built pin map: docs/DUAL_PANEL.md. Bus protocol: docs/RS485_PROTOCOL.md.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/structs/vreg_and_chip_reset.h"
#include "hardware/structs/watchdog.h"
#include "fw_layout.h"
#include "panel_config.h"
#include "ws2812.pio.h"

// ── As-built pin map (docs/DUAL_PANEL.md → "RP2040 (U306) GPIO map") ─────────
#define PIN_RS485_TX    0
#define PIN_RS485_RX    1
#define PIN_RS485_DE    4    // DE + ~RE tied; HIGH = transmit
#define PIN_TERM_SENSE  10   // LOW = terminated (SW202)
#define PIN_LED_DATA    11   // → U301 shifter → carrier
#define PIN_DEBUG_LED   16   // carrier D202
#define PIN_SENSE_12V   17   // divider, DIGITAL. Pulls must be OFF.
#define PIN_DIP_ID3     18   // GPIO18 is bit 3, GPIO21 bit 0
#define PIN_DIP_ID2     19
#define PIN_DIP_ID1     20
#define PIN_DIP_ID0     21
#define PIN_INT_OUT     22   // emulated open-drain, sole gameplay press path

#define NUM_FSR   4
#define NUM_LEDS  25
_Static_assert(NUM_FSR == KP_NUM_FSR, "panel_config.h describes four channels");

// Channel i = ADC i = GPIO 26+i, named by the panel edge it lands on.
enum { FSR_SOUTH = 0, FSR_WEST = 1, FSR_NORTH = 2, FSR_EAST = 3 };
static const uint  FSR_GPIO[NUM_FSR] = {26, 27, 28, 29};
static const char *FSR_NAME[NUM_FSR] = {"South", "West", "North", "East"};

// ── Tunables ────────────────────────────────────────────────────────────────
// Thresholds and channel flags are config: boot defaults in panel_config.h,
// then the master's 'W' (below).
// A state change must hold for this many consecutive samples (~0.1 ms at the
// ~100 kHz loop rate) before it is committed.
#define FSR_PERSISTENCE        10

// Panel-level debounce on the press output (INT, the gameplay path), both
// edges (the user, 2026-10-22): the OR of the channels must hold a new state
// this long before INT follows it. At panel level, weight passing from one
// sensor to another doesn't release the panel. A constant delay, so a game's
// global offset absorbs the press side. Fixed for now; meant to become a pad
// config field (docs/PANEL_CONFIG.md → "Press debounce").
#define PRESS_DEBOUNCE_US      2000
#define RELEASE_DEBOUNCE_US    2000

#define LED_FALLBACK_MS   100   // no 'L' for this long -> normal mode goes dark
#define LED_REFRESH_MS    16
#define ID_BOOT_MS        3000
#define ID_WHITE          96

// No master (the user, 2026-10-22): no valid bus frame of any kind for
// NO_MASTER_MS, so normal mode shows the panel has 12V with a slow dim white
// breath, and all white (ID_WHITE) while INT is asserted, so a press can be
// checked without a master. A running master always sends 'L' frames, so this
// only shows with the master off, unplugged or restarting. No timeout.
#define NO_MASTER_MS      1000
#define STANDBY_PERIOD_MS 4000  // one breath
#define STANDBY_PEAK      24    // of 255

// 24 levels alone step visibly at the dim end (2026-10-23: "choppy"; 1 -> 2 is
// twice the light), so the breath runs in 1/NUM_LEDS steps: every LED at the
// whole level, and this many more, in this order, one level up. The order puts
// each next LED where the lit ones are least crowded (sum of 1/d^2 on the
// lattice in docs/ANIMATIONS.md, from the centre), so the extra light spreads
// across the panel instead of gathering in one corner.
static const uint8_t STANDBY_DITHER[NUM_LEDS] = {
    12, 0, 24, 3, 21, 1, 23, 10, 14, 2, 22, 7, 17,
    8, 16, 9, 20, 6, 18, 4, 15, 5, 19, 11, 13,
};

// Test mode: an edge stays dark up to this many counts above its tare, then
// brightness follows its calibrated value to full at 1000. Deliberately NOT the
// press threshold, which the user can move. Was a fixed 200 raw before tare
// existed (100 before that); tare + 20 is the user's guess at what the stock
// pad's sensor test does (2026-10-22).
#define TEST_MARGIN       20

// Tare: each sensor's tracked rest value (docs/PANEL_CONFIG.md → "Tare and
// calibrated values"). Core 1 samples every sensor once a millisecond and
// judges it in windows. A window counts only when no channel on the panel is
// pressed, nothing was released for TARE_HOLDOFF_MS, and the sensor is steady.
// The tare then falls to a lower reading at once (an inflated tare makes
// presses harder) and rises towards a higher one slowly, by at most
// TARE_MAX_RISE above itself (more than that is a foot, not drift).
#define TARE_WINDOW        64     // 1 ms samples per window
#define TARE_MAX_SPAN      64     // max - min within a window: a spike, skip it
#define TARE_MAX_STEP      8      // window mean vs the last one: still moving
#define TARE_MAX_RISE      48
#define TARE_RISE_SHIFT    7      // 1/128 of the gap per window: ~8 s to close it
#define TARE_HOLDOFF_MS    1000
#define TARE_BOOT_MS       500    // ADC and preload settle before the first tare
#define TARE_DEFAULT       100    // until a sensor's first steady window (~rest in the pad)
#define TARE_MAX           1000   // a sensor resting above this is faulty, not tared

#define SENSE_RISE_STABLE_MS   30   // slow to trust the rail, fast to give up on it
#define SENSE_FALL_STABLE_MS   2

// ── LED images ──────────────────────────────────────────────────────────────
// Indices are wire (serpentine) order, 0-based. Physical layout, as indices:
//
//    0   1   2   3
//      6   5   4
//    7   8   9  10
//     13  12  11
//   14  15  16  17
//     20  19  18
//   21  22  23  24
//
// Indexed by panel ID: 0 UL, 1 U, 2 UR, 3 L, 4 C, 5 R, 6 DL, 7 D, 8 DR.
#define IMG_END 0xFF
static const uint8_t ID_IMAGE[9][6] = {
    { 0,  1,  6,  7, IMG_END},          // UL
    { 1,  2,  5, IMG_END},              // U
    { 2,  3,  4, 10, IMG_END},          // UR
    { 7, 13, 14, IMG_END},              // L
    { 8,  9, 12, 15, 16, IMG_END},      // C
    {10, 11, 17, IMG_END},              // R
    {14, 20, 21, 22, IMG_END},          // DL
    {19, 22, 23, IMG_END},              // D
    {17, 18, 23, 24, IMG_END},          // DR
};
// DIP is not a panel ID: both diagonals.
static const uint8_t X_IMAGE[] = {0, 3, 4, 6, 8, 9, 12, 15, 16, 18, 20, 21, 24, IMG_END};

// Test mode: each edge reuses the ID image of the panel on that side.
static const uint8_t *const EDGE_IMAGE[NUM_FSR] = {
    [FSR_SOUTH] = ID_IMAGE[7],
    [FSR_WEST]  = ID_IMAGE[3],
    [FSR_NORTH] = ID_IMAGE[1],
    [FSR_EAST]  = ID_IMAGE[5],
};
// The stock pad's sensor-test colours (docs/PANEL_CONFIG.md → "Sensor test").
static const uint8_t EDGE_RGB[NUM_FSR][3] = {
    [FSR_SOUTH] = {1, 1, 0},   // yellow
    [FSR_WEST]  = {1, 0, 0},   // red
    [FSR_NORTH] = {0, 0, 1},   // blue
    [FSR_EAST]  = {0, 1, 0},   // green
};

enum { MODE_NORMAL = 0, MODE_ID = 1, MODE_TEST = 2, MODE_COUNT };
static const char *MODE_NAME[MODE_COUNT] = {"normal", "ID", "test"};

// ── Shared state ────────────────────────────────────────────────────────────
// Core 0 writes: raw values, mask, INT state. Core 1 writes: the config, the
// INT enable/hold flags and identify requests. Each field has one writer.
static volatile uint16_t g_fsr_raw[NUM_FSR];
static volatile uint16_t g_tare[NUM_FSR];         // core 1 writes
static volatile uint8_t  g_pressed_mask = 0;
static volatile bool     g_int_asserted = false;
static volatile bool     g_int_enable = false;  // set when bus mode starts
static volatile bool     g_int_hold   = false;  // force released (reboot path)
static volatile uint8_t  g_ident_req_ms = 0;    // core 1 sets, core 0 clears

static uint8_t g_dip = 15;   // read once at boot

// ── Config ──────────────────────────────────────────────────────────────────
// The config in use (calibrated thresholds): the boot defaults, then each
// accepted 'W'. Only core 1 reads it.
typedef struct {
    uint16_t press[NUM_FSR];              // calibrated
    uint16_t rel[NUM_FSR];
    bool     active[NUM_FSR];             // populated && enabled: detects presses
    uint8_t  wire[KP_PANEL_CFG_LEN];      // as received, for the 'w' CRC
} panel_cfg_t;
static panel_cfg_t g_cfg;
static bool g_configured = false;         // a 'W' accepted since boot (core 1)

// What core 0 compares against: the config's thresholds in raw counts at the
// current tares, rebuilt whenever either changes. Core 1 fills the spare copy
// and only then swaps the pointer, and core 0 reads the pointer once per pass,
// so a pass never mixes old and new values (a new press threshold with the old
// release could put release above press). Rebuilds are at least one tare
// window (64 ms) or one 'W' (260 us on the wire) apart, against a ~10 us pass.
typedef struct {
    uint16_t press[NUM_FSR];              // raw counts
    uint16_t rel[NUM_FSR];
    bool     active[NUM_FSR];
} thresholds_t;
static thresholds_t g_thr_copy[2];
static const thresholds_t *volatile g_thr = &g_thr_copy[0];

static void thresholds_rebuild(void) {
    thresholds_t *next = (g_thr == &g_thr_copy[0]) ? &g_thr_copy[1] : &g_thr_copy[0];
    for (int i = 0; i < NUM_FSR; i++) {
        next->press[i]  = kp_cal_to_raw(g_cfg.press[i], g_tare[i]);
        next->rel[i]    = kp_cal_to_raw(g_cfg.rel[i], g_tare[i]);
        next->active[i] = g_cfg.active[i];
    }
    __dmb();   // the copy is complete before core 0 can see the pointer
    g_thr = next;
}

// `wire` is one panel's 20 bytes, already checked by kp_panel_cfg_bad_channel().
static void config_apply(const uint8_t *wire) {
    memcpy(g_cfg.wire, wire, KP_PANEL_CFG_LEN);
    for (int i = 0; i < NUM_FSR; i++) {
        const uint8_t *ch = wire + i * KP_CH_LEN;
        g_cfg.press[i]  = kp_get_u16(ch);
        g_cfg.rel[i]    = kp_get_u16(ch + 2);
        g_cfg.active[i] = (ch[4] & (KP_CH_POPULATED | KP_CH_ENABLED)) ==
                          (KP_CH_POPULATED | KP_CH_ENABLED);
    }
    thresholds_rebuild();
}

static void config_init_defaults(void) {
    for (int i = 0; i < NUM_FSR; i++) g_tare[i] = TARE_DEFAULT;
    uint8_t wire[KP_PANEL_CFG_LEN];
    kp_panel_cfg_default(wire);
    config_apply(wire);
}

// ── INT_OUT: emulated open-drain ────────────────────────────────────────────
// Assert = drive LOW; release = input (hi-Z), the master's pull-up does the
// rest. Never drive HIGH. gpio_disable_pulls() is mandatory: pads reset with
// the pull-DOWN on, which fights the master's 10k (docs/PANEL_BRINGUP.md).
static void int_out_init(void) {
    gpio_init(PIN_INT_OUT);
    gpio_disable_pulls(PIN_INT_OUT);
    gpio_put(PIN_INT_OUT, 0);
    gpio_set_dir(PIN_INT_OUT, GPIO_IN);
}

// Closed (ON) switch reads 0, so ID 0 is all four ON and all-OFF reads 15.
static uint8_t dip_read(void) {
    return (uint8_t)((gpio_get(PIN_DIP_ID3) << 3) |
                     (gpio_get(PIN_DIP_ID2) << 2) |
                     (gpio_get(PIN_DIP_ID1) << 1) |
                      gpio_get(PIN_DIP_ID0));
}

// ── Core 0: FSR sampling + INT ──────────────────────────────────────────────
static void fsr_loop(void) {
    bool     pressed[NUM_FSR] = {false};
    uint8_t  streak[NUM_FSR]  = {0};
    uint32_t ident_start = 0, ident_len = 0;
    bool     panel_on = false;            // debounced: what INT shows
    bool     panel_pending = false;       // the channels disagree with panel_on
    uint32_t panel_pending_since = 0;

    while (true) {
        const thresholds_t *cfg = g_thr;
        uint8_t mask = 0;
        for (int i = 0; i < NUM_FSR; i++) {
            adc_select_input(i);
            uint16_t v = adc_read() & 0x0FFF;
            g_fsr_raw[i] = v;
            // A disabled or unpopulated channel still reports its raw value
            // but never presses; disabling a pressed channel releases it.
            if (!cfg->active[i]) { pressed[i] = false; streak[i] = 0; continue; }
            bool want = pressed[i];
            if (pressed[i]) { if (v <= cfg->rel[i])   want = false; }
            else            { if (v >= cfg->press[i]) want = true;  }
            if (want != pressed[i]) {
                if (++streak[i] >= FSR_PERSISTENCE) { pressed[i] = want; streak[i] = 0; }
            } else {
                streak[i] = 0;
            }
            if (pressed[i]) mask |= (uint8_t)(1u << i);
        }
        g_pressed_mask = mask;   // per channel, not debounced: telemetry

        uint32_t now = time_us_32();
        bool raw_on = mask != 0;
        if (raw_on == panel_on) {
            panel_pending = false;
        } else if (!panel_pending) {
            panel_pending = true;
            panel_pending_since = now;
        } else if (now - panel_pending_since >=
                   (raw_on ? PRESS_DEBOUNCE_US : RELEASE_DEBOUNCE_US)) {
            panel_on = raw_on;
            panel_pending = false;
        }

        uint8_t req = g_ident_req_ms;
        if (req) { ident_start = now; ident_len = req * 1000u; g_ident_req_ms = 0; }
        bool ident = ident_len && (now - ident_start) < ident_len;
        if (!ident) ident_len = 0;

        bool assert_int = g_int_enable && !g_int_hold && (panel_on || ident);
        if (assert_int != g_int_asserted) {
            gpio_set_dir(PIN_INT_OUT, assert_int ? GPIO_OUT : GPIO_IN);
            g_int_asserted = assert_int;
        }
    }
}

// ── Status / build identity ─────────────────────────────────────────────────
#define RST_POR        (1u << 0)
#define RST_RUN        (1u << 1)
#define RST_PSM        (1u << 2)
#define RST_WD_TIMER   (1u << 3)
#define RST_WD_FORCE   (1u << 4)
static uint8_t  g_reset_flags = 0;
static uint32_t g_build_tag   = 0;

static void capture_reset_cause(void) {
    uint32_t cr = vreg_and_chip_reset_hw->chip_reset;
    uint32_t wd = watchdog_hw->reason;
    if (cr & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS)         g_reset_flags |= RST_POR;
    if (cr & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_RUN_BITS)         g_reset_flags |= RST_RUN;
    if (cr & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_PSM_RESTART_BITS) g_reset_flags |= RST_PSM;
    if (wd & WATCHDOG_REASON_TIMER_BITS)                          g_reset_flags |= RST_WD_TIMER;
    if (wd & WATCHDOG_REASON_FORCE_BITS)                          g_reset_flags |= RST_WD_FORCE;
}

// Same image -> same tag; the master's 'H' shows it per panel.
static void compute_build_tag(void) {
    const char *s = __DATE__ " " __TIME__;
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    g_build_tag = h;
}

static void put_u16_sat(uint8_t *p, uint32_t v) {
    if (v > 0xFFFF) v = 0xFFFF;
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

// ── Everything below runs on core 1 ─────────────────────────────────────────

// ── Tare tracking (see the TARE_ tunables) ──────────────────────────────────
static uint32_t g_tare_q4[NUM_FSR];       // tare in 1/16 counts, for slow rises
static bool     g_tare_set[NUM_FSR];      // a steady window has been seen
static uint32_t g_last_pressed_ms = 0;

static void tare_tick(uint32_t now_ms) {
    static uint32_t sum[NUM_FSR];
    static uint16_t lo[NUM_FSR], hi[NUM_FSR], prev_mean[NUM_FSR];
    static uint16_t n = 0;

    if (g_pressed_mask) g_last_pressed_ms = now_ms;
    for (int i = 0; i < NUM_FSR; i++) {
        uint16_t v = g_fsr_raw[i];
        if (n == 0) { sum[i] = 0; lo[i] = hi[i] = v; }
        sum[i] += v;
        if (v < lo[i]) lo[i] = v;
        if (v > hi[i]) hi[i] = v;
    }
    if (++n < TARE_WINDOW) return;
    n = 0;

    bool idle = !g_pressed_mask && now_ms >= TARE_BOOT_MS &&
                (now_ms - g_last_pressed_ms) >= TARE_HOLDOFF_MS;
    bool changed = false;
    for (int i = 0; i < NUM_FSR; i++) {
        uint16_t mean = (uint16_t)(sum[i] / TARE_WINDOW);
        uint16_t step = mean > prev_mean[i] ? mean - prev_mean[i] : prev_mean[i] - mean;
        prev_mean[i] = mean;
        if (!idle || hi[i] - lo[i] > TARE_MAX_SPAN || step > TARE_MAX_STEP ||
            mean > TARE_MAX) continue;
        uint32_t target = (uint32_t)mean << 4;
        if (!g_tare_set[i] || target <= g_tare_q4[i]) {
            g_tare_q4[i] = target;
            g_tare_set[i] = true;
        } else if (mean - g_tare[i] <= TARE_MAX_RISE) {
            uint32_t inc = (target - g_tare_q4[i]) >> TARE_RISE_SHIFT;
            g_tare_q4[i] += inc ? inc : 1;
        }
        uint16_t t = (uint16_t)(g_tare_q4[i] >> 4);
        if (t != g_tare[i]) { g_tare[i] = t; changed = true; }
    }
    if (changed) thresholds_rebuild();
}

// ── SENSE_12V filter (polled from the core 1 loop) ──────────────────────────
static bool g_12v_present = false;

static void sense_12v_poll(uint32_t now_ms) {
    static bool     pending = false;
    static uint32_t since   = 0;
    bool raw = gpio_get(PIN_SENSE_12V);
    if (raw == g_12v_present) { pending = false; return; }
    if (!pending) { pending = true; since = now_ms; return; }
    uint32_t need = raw ? SENSE_RISE_STABLE_MS : SENSE_FALL_STABLE_MS;
    if (now_ms - since >= need) { g_12v_present = raw; pending = false; }
}

// ── LEDs: PIO fed by DMA ────────────────────────────────────────────────────
// A blocking paint is ~750 us of PIO writes, during which a poll would be
// answered late and collide with the master's next frame. DMA makes a paint
// cost nothing on this core. Gated on SENSE_12V: driving WS2815 DIN with their
// 12V rail dead forward-biases their inputs.
static PIO      led_pio = pio0;
static uint     led_sm;
static int      led_dma = -1;
static uint32_t led_words[NUM_LEDS];
static uint32_t led_last_start_us = 0;

// 25 words x 30 us on the wire, then WS2815 reset (~280 us) before the next.
#define LED_FRAME_GAP_US  (NUM_LEDS * 30u + 300u + 150u)

static void led_init(void) {
    uint offset = pio_add_program(led_pio, &ws2812_program);
    led_sm = pio_claim_unused_sm(led_pio, true);
    ws2812_program_init(led_pio, led_sm, offset, PIN_LED_DATA, 800000.0f, false);

    led_dma = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(led_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(led_pio, led_sm, true));
    dma_channel_configure(led_dma, &c, &led_pio->txf[led_sm], led_words, NUM_LEDS, false);
}

static bool led_free(void) {
    return led_dma >= 0 && !dma_channel_is_busy(led_dma) &&
           (time_us_32() - led_last_start_us) >= LED_FRAME_GAP_US;
}

static inline uint32_t grb_word(uint8_t r, uint8_t g, uint8_t b) {
    return (((uint32_t)g << 16) | ((uint32_t)r << 8) | b) << 8u;
}

static void led_start(void) {
    led_last_start_us = time_us_32();
    dma_channel_transfer_from_buffer_now(led_dma, led_words, NUM_LEDS);
}

static void draw_image(const uint8_t *img, uint8_t r, uint8_t g, uint8_t b) {
    for (; *img != IMG_END; img++) led_words[*img] = grb_word(r, g, b);
}

// ── RS-485 responder ────────────────────────────────────────────────────────
// Same framing, commands and rules as bringup/main.c, whose comments carry the
// reasoning: never printf on the reply path, RX is IRQ-driven and the ISR
// always drains the FIFO, DE drops on BUSY plus a character of slack.
#define RS485_BAUD      1000000u
#define RS485_SYNC      0x55
#define RS485_BCAST     0xFF
#define RS485_MAX_PAY   80
#define RS_RING         2048
// A gap this long mid-packet abandons the packet. Longer than the 32-byte RX
// FIFO takes to fill (320 us), so IRQ latency alone can't fake one without also
// overrunning the FIFO; shorter than any gap the master leaves between packets.
#define RS485_GAP_US    500u
#define RS_GAP_FLAG     0x100u          // ring entry: idle gap before this byte

static bool g_rs485_on = false;

static volatile uint16_t rs_ring[RS_RING];
static volatile uint16_t rs_head = 0, rs_tail = 0;
static volatile uint32_t rs_overruns = 0, rs_uart_errs = 0, rs_turnaround = 0;
static volatile bool     rs_resync = false;
static uint32_t rs_last_tx_us = 0;
static uint32_t rs_last_rx_us = 0;      // ISR only
static uint32_t rs_frames = 0, rs_crc_errs = 0, rs_polls = 0, rs_led = 0, rs_timeouts = 0;
static uint32_t rs_cfg_ok = 0, rs_cfg_rejected = 0;

// LED mode state
static uint8_t  g_mode        = MODE_NORMAL;
static uint8_t  g_base_mode   = MODE_NORMAL;   // what a timed mode returns to
static uint32_t g_mode_end_ms = 0;             // 0 = until changed
static bool     g_boot_id_done = false;
static uint8_t  g_led_frame[3 * NUM_LEDS];
static uint32_t g_led_frame_ms = 0;
static bool     g_led_frame_seen = false;
static uint32_t g_bus_frame_ms = 0;            // last valid frame, any address
static bool     g_led_dirty = false;

static void set_mode(uint8_t mode, uint32_t duration_ms) {
    g_mode = mode;
    g_mode_end_ms = duration_ms ? to_ms_since_boot(get_absolute_time()) + duration_ms : 0;
    if (g_mode_end_ms == 0 && duration_ms) g_mode_end_ms = 1;
    g_led_dirty = true;
}

static void rs485_rx_irq(void) {
    // Timed per IRQ, not per byte: one drain of a contiguous stream is never
    // more than a FIFO-fill apart from the last, well under RS485_GAP_US.
    uint32_t now = time_us_32();
    uint16_t flag = (now - rs_last_rx_us) > RS485_GAP_US ? RS_GAP_FLAG : 0;
    rs_last_rx_us = now;
    while (uart_is_readable(uart0)) {
        uint32_t dr = uart_get_hw(uart0)->dr;
        if (dr & 0xF00u) {          // FE | PE | BE | OE
            if ((time_us_32() - rs_last_tx_us) < 200u) rs_turnaround++;
            else                                       rs_uart_errs++;
            rs_resync = true;
        }
        uint16_t next = (uint16_t)((rs_head + 1u) % RS_RING);
        if (next == rs_tail) { rs_overruns++; continue; }   // drop the byte, never the drain
        rs_ring[rs_head] = (uint16_t)((dr & 0xFF) | flag);
        rs_head = next;
        flag = 0;
    }
}

static uint8_t crc8_update(uint8_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    return crc;
}

static void rs485_send(uint8_t cmd, uint8_t addr, const uint8_t *pay, uint8_t len) {
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
    uart_tx_wait_blocking(uart0);
    busy_wait_us(12);
    gpio_put(PIN_RS485_DE, 0);
    rs_last_tx_us = time_us_32();
}

static void rs485_clear_counters(void) {
    rs_frames = rs_crc_errs = rs_polls = rs_led = rs_timeouts = 0;
    rs_cfg_ok = rs_cfg_rejected = 0;
    rs_overruns = rs_uart_errs = rs_turnaround = 0;
}

static void reboot_into_bootloader(void) {
    g_int_hold = true;
    busy_wait_us(100);                  // core 0 releases INT within one pass
    gpio_put(PIN_RS485_DE, 0);
    watchdog_hw->scratch[0] = FW_HOLD_MAGIC;
    watchdog_reboot(0, 0, 10);
    while (true) tight_loop_contents();
}

static void rs485_handle(uint8_t cmd, uint8_t addr, const uint8_t *pay, uint8_t len) {
    uint8_t me = g_dip;
    if (addr != me && addr != RS485_BCAST) return;

    switch (cmd) {
    case 'F': {
        uint8_t r[KP_F_LEN];
        for (int i = 0; i < NUM_FSR; i++) {
            kp_put_u16(&r[i * 2], g_fsr_raw[i]);
            kp_put_u16(&r[KP_F_TARE + i * 2], g_tare[i]);
        }
        r[KP_F_MASK] = (uint8_t)(g_pressed_mask | (g_configured ? 0 : KP_F_NOT_CONFIGURED));
        rs485_send('f', me, r, sizeof(r));
        rs_polls++;
        break;
    }
    case 'I': {
        uint8_t ms = len ? pay[0] : 2;
        if (ms < 1)  ms = 1;
        if (ms > 50) ms = 50;
        // Ack first, then the pulse, which core 0 times without blocking us.
        rs485_send('i', me, NULL, 0);
        busy_wait_us(50);
        g_ident_req_ms = ms;
        break;
    }
    case 'W': {
        // Whole panel config, applied only whole and only when addressed (a
        // broadcast would have nine panels answer at once). The reply carries
        // the CRC of the config now in use, accepted or not, so the master
        // checks the values rather than trusting the status.
        if (addr != me) break;
        uint8_t status;
        if (len >= 1 && pay[0] != KP_CONFIG_VERSION)
            status = KP_WACK_UNKNOWN_VERSION;
        else if (len != KP_W_LEN || kp_panel_cfg_bad_channel(pay + 1) >= 0)
            status = KP_WACK_INVALID;
        else
            status = KP_WACK_APPLIED;
        if (status == KP_WACK_APPLIED) {
            config_apply(pay + 1);
            g_configured = true;
            rs_cfg_ok++;
        } else {
            rs_cfg_rejected++;
        }
        uint8_t r[KP_WACK_LEN];
        r[0] = status;
        r[1] = KP_CONFIG_VERSION;
        put_u32(&r[2], kp_w_crc(g_cfg.wire));
        rs485_send('w', me, r, sizeof(r));
        break;
    }
    case 'H': {
        uint8_t r[24];
        put_u32(&r[0], g_build_tag);
        put_u32(&r[4], to_ms_since_boot(get_absolute_time()) / 1000u);
        r[8] = g_reset_flags;
        r[9] = (uint8_t)((g_12v_present              ? 1u << 0 : 0) |
                         (!gpio_get(PIN_TERM_SENSE)   ? 1u << 1 : 0) |
                         (g_int_enable                ? 1u << 2 : 0) |
                         (g_mode == MODE_NORMAL       ? 1u << 3 : 0) |
                         (g_int_asserted              ? 1u << 4 : 0) |
                         ((g_mode & 0x3u) << 5));
        put_u16_sat(&r[10], rs_crc_errs);
        put_u16_sat(&r[12], rs_uart_errs);
        put_u16_sat(&r[14], rs_overruns);
        put_u16_sat(&r[16], rs_turnaround);
        put_u32(&r[18], rs_frames);
        put_u16_sat(&r[22], rs_timeouts);
        rs485_send('h', me, r, sizeof(r));
        break;
    }
    case 'Z':
        rs485_clear_counters();
        if (addr == me) rs485_send('z', me, NULL, 0);
        break;
    case 'M':
        // [mode][seconds, 0 = until changed]
        if (len >= 1 && pay[0] < MODE_COUNT) {
            set_mode(pay[0], len >= 2 ? pay[1] * 1000u : 0);
            if (addr == me) rs485_send('m', me, pay, len);
        }
        break;
    case FW_CMD_HOLD:
        if (addr == me) rs485_send(FW_CMD_HOLD_ACK, me, NULL, 0);
        reboot_into_bootloader();
        break;
    case 'L':
        rs_led++;
        if (len == sizeof(g_led_frame)) {
            memcpy(g_led_frame, pay, len);
            g_led_frame_ms = to_ms_since_boot(get_absolute_time());
            g_led_frame_seen = true;
            g_led_dirty = true;
        }
        break;
    default:
        break;
    }
}

static void rs485_service(void) {
    static enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
    static uint8_t cmd, addr, len, idx, crc, pay[RS485_MAX_PAY];

    if (rs_resync) { rs_resync = false; st = W_SYNC; }

    while (rs_tail != rs_head) {
        uint16_t e = rs_ring[rs_tail];
        uint8_t  c = (uint8_t)e;
        rs_tail = (uint16_t)((rs_tail + 1u) % RS_RING);
        // A packet never pauses mid-way. Without this, garbage that parses as
        // a length byte swallows the next real packets as its payload.
        if ((e & RS_GAP_FLAG) && st != W_SYNC) { rs_timeouts++; st = W_SYNC; }
        switch (st) {
        case W_SYNC: if (c == RS485_SYNC) { crc = 0; st = W_CMD; } break;
        case W_CMD:  cmd  = c; crc = crc8_update(crc, c); st = W_ADDR; break;
        case W_ADDR: addr = c; crc = crc8_update(crc, c); st = W_LEN;  break;
        case W_LEN:
            len = c; crc = crc8_update(crc, c); idx = 0;
            if (len > RS485_MAX_PAY) { rs_crc_errs++; st = W_SYNC; }
            else st = len ? W_PAY : W_CRC;
            break;
        case W_PAY:
            pay[idx++] = c; crc = crc8_update(crc, c);
            if (idx >= len) st = W_CRC;
            break;
        case W_CRC:
            if (c == crc) {
                rs_frames++;
                g_bus_frame_ms = to_ms_since_boot(get_absolute_time());
                rs485_handle(cmd, addr, pay, len);
            } else {
                rs_crc_errs++;
            }
            st = W_SYNC;
            break;
        }
    }
}

// Must run on core 1 so the UART IRQ is routed here, not to the sampling loop.
static void rs485_start(void) {
    uart_init(uart0, RS485_BAUD);
    gpio_set_function(PIN_RS485_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RS485_RX, GPIO_FUNC_UART);
    gpio_pull_up(PIN_RS485_RX);         // idle-mark through our own DE window
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    uart_set_hw_flow(uart0, false, false);
    gpio_put(PIN_RS485_DE, 0);
    while (uart_is_readable(uart0)) (void)uart_get_hw(uart0)->dr;   // stale FIFO
    uart_get_hw(uart0)->rsr = 0x0Fu;
    rs_head = rs_tail = 0;
    rs_last_tx_us = rs_last_rx_us = time_us_32();
    irq_set_exclusive_handler(UART0_IRQ, rs485_rx_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
    g_bus_frame_ms = to_ms_since_boot(get_absolute_time());   // no-master clock starts here
    g_rs485_on = true;
}

// One breath: dark, up, down, dark. Squared, so it eases in at the dim end
// instead of stepping up out of black. In 1/NUM_LEDS of a level (STANDBY_DITHER).
static uint32_t standby_level(uint32_t now_ms) {
    uint32_t half = STANDBY_PERIOD_MS / 2;
    uint32_t t = now_ms % STANDBY_PERIOD_MS;
    if (t > half) t = STANDBY_PERIOD_MS - t;
    return (uint32_t)((uint64_t)STANDBY_PEAK * NUM_LEDS * t * t / ((uint64_t)half * half));
}

// ── Rendering ───────────────────────────────────────────────────────────────
static void render(uint32_t now_ms) {
    memset(led_words, 0, sizeof(led_words));
    switch (g_mode) {
    case MODE_ID:
        draw_image(g_dip <= 8 ? ID_IMAGE[g_dip] : X_IMAGE, ID_WHITE, ID_WHITE, ID_WHITE);
        break;
    case MODE_TEST:
        for (int i = 0; i < NUM_FSR; i++) {
            uint16_t v = g_fsr_raw[i], t = g_tare[i];
            if (v <= t + TEST_MARGIN) continue;
            uint8_t lvl = (uint8_t)((uint32_t)kp_raw_to_cal(v, t) * 255u / KP_CAL_MAX);
            draw_image(EDGE_IMAGE[i], EDGE_RGB[i][0] * lvl, EDGE_RGB[i][1] * lvl,
                       EDGE_RGB[i][2] * lvl);
        }
        break;
    default:
        // Signed: now_ms is read before rs485_service() stamps a new frame, so
        // a frame can be up to a tick "newer" than now. Unsigned, that wrapped
        // to ~4e9 and blanked fresh frames: whole-panel blinking in episodes,
        // as the master and panel clocks drifted through phase (2026-10-19).
        if (g_led_frame_seen && (int32_t)(now_ms - g_led_frame_ms) < LED_FALLBACK_MS) {
            for (int i = 0; i < NUM_LEDS; i++)
                led_words[i] = grb_word(g_led_frame[i * 3], g_led_frame[i * 3 + 1],
                                        g_led_frame[i * 3 + 2]);
        } else if (g_rs485_on && (int32_t)(now_ms - g_bus_frame_ms) >= NO_MASTER_MS) {
            uint32_t fine = g_int_asserted ? ID_WHITE * NUM_LEDS : standby_level(now_ms);
            uint8_t lvl = (uint8_t)(fine / NUM_LEDS), up = lvl + 1;
            for (int i = 0; i < NUM_LEDS; i++) led_words[i] = grb_word(lvl, lvl, lvl);
            for (uint32_t i = 0; i < fine % NUM_LEDS; i++)
                led_words[STANDBY_DITHER[i]] = grb_word(up, up, up);
        }
        break;
    }
}

// ── USB console (bench only; printf here delays replies, see bringup) ───────
static void banner(void) {
    printf("\n========================================================\n");
    printf(" dual-panel gameplay firmware  (built %s %s)\n", __DATE__, __TIME__);
    printf("========================================================\n");
    printf("  build tag : %08lX\n", (unsigned long)g_build_tag);
#if PANEL_UNDER_BOOTLOADER
    printf("  image     : at 0x%08X, behind panel_bootloader\n", XIP_BASE + FW_APP_OFFSET);
#else
    printf("  image     : standalone at 0x%08X, no bootloader\n", XIP_BASE);
#endif
    printf("  DIP       : %u (%s)\n", g_dip,
           g_dip <= 8 ? "panel ID" : g_dip == 10 ? "sensor pressure test" : "not a panel ID");
    printf("  '?' for commands\n");
}

static void help(void) {
    printf("\n  s  status            v  banner\n"
           "  n  LED mode: normal   i  LED mode: ID   t  LED mode: test\n"
           "  g  send a broken header onto the bus (master paused with x first)\n"
           "  B  reboot into BOOTSEL\n"
           "  U  reboot into the panel bootloader and hold\n");
}

// Bench test of the master's parser resync (docs/MASTER_BRINGUP.md): a lone
// header claiming an 80-byte payload, then silence. The master's `x` must
// pause its traffic first, or this lands on top of it. The other panels hear
// it too, and each counts one parser timeout.
static void send_broken_header(void) {
    if (!g_rs485_on) { printf("\n  bus not started (no 12V, or DIP not 0-8)\n"); return; }
    const uint8_t hdr[4] = { RS485_SYNC, 'X', g_dip, RS485_MAX_PAY };
    gpio_put(PIN_RS485_DE, 1);          // same DE handling as rs485_send()
    busy_wait_us(2);
    uart_write_blocking(uart0, hdr, sizeof(hdr));
    uart_tx_wait_blocking(uart0);
    busy_wait_us(12);
    gpio_put(PIN_RS485_DE, 0);
    rs_last_tx_us = time_us_32();
    printf("\n  sent 55 58 %02X 50 and nothing after it\n", g_dip);
}

static void status(void) {
    printf("\n  12V %s, DIP %u, %s, bus %s, INT %s%s, LED mode %s\n",
           g_12v_present ? "present" : "ABSENT", g_dip,
           !gpio_get(PIN_TERM_SENSE) ? "terminated" : "not terminated",
           g_rs485_on ? "on" : "off", g_int_enable ? "enabled" : "off",
           g_int_asserted ? " (asserted)" : "", MODE_NAME[g_mode]);
    if (g_rs485_on) {
        uint32_t quiet = to_ms_since_boot(get_absolute_time()) - g_bus_frame_ms;
        printf("  master    : last valid bus frame %lu ms ago%s\n", (unsigned long)quiet,
               quiet >= NO_MASTER_MS ? " (no master)" : "");
    }
    printf("  FSR       : raw / tare / calibrated\n");
    for (int i = 0; i < NUM_FSR; i++)
        printf("    %-5s %4u / %4u%s / %5d%s\n", FSR_NAME[i], g_fsr_raw[i], g_tare[i],
               g_tare_set[i] ? "" : " (default, no steady window yet)",
               kp_raw_to_cal(g_fsr_raw[i], g_tare[i]),
               (g_pressed_mask >> i) & 1 ? "  PRESSED" : "");
    printf("  config    : %s, CRC %08lX (%lu 'W' accepted, %lu rejected)\n",
           g_configured ? "from the master" : "boot defaults, no 'W' yet",
           (unsigned long)kp_w_crc(g_cfg.wire), (unsigned long)rs_cfg_ok,
           (unsigned long)rs_cfg_rejected);
    const thresholds_t *thr = g_thr;
    for (int i = 0; i < NUM_FSR; i++) {
        uint8_t fl = g_cfg.wire[i * KP_CH_LEN + 4];
        printf("    %-5s press %4u release %4u (raw %4u / %4u)  %s%s\n", FSR_NAME[i],
               g_cfg.press[i], g_cfg.rel[i], thr->press[i], thr->rel[i],
               fl & KP_CH_POPULATED ? "populated" : "NOT populated",
               fl & KP_CH_ENABLED ? "" : ", disabled");
    }
    printf("  bus       : %lu frames, %lu crc, %lu uart, %lu overrun, %lu turnaround,"
           " %lu timeout, %lu 'F', %lu 'L'\n",
           (unsigned long)rs_frames, (unsigned long)rs_crc_errs,
           (unsigned long)rs_uart_errs, (unsigned long)rs_overruns,
           (unsigned long)rs_turnaround, (unsigned long)rs_timeouts,
           (unsigned long)rs_polls, (unsigned long)rs_led);
}

static void console_poll(void) {
    static bool was_connected = false;
    bool connected = stdio_usb_connected();
    if (connected && !was_connected) banner();
    was_connected = connected;
    if (!connected) return;

    int c = getchar_timeout_us(0);
    switch (c) {
    case 's': status(); break;
    case 'v': banner(); break;
    case 'n': set_mode(MODE_NORMAL, 0); printf("\n  LED mode normal\n"); break;
    case 'i': set_mode(MODE_ID, 0);     printf("\n  LED mode ID\n");     break;
    case 't': set_mode(MODE_TEST, 0);   printf("\n  LED mode test\n");   break;
    case 'g': send_broken_header(); break;
    case 'B':
        printf("\n  rebooting into BOOTSEL\n");
        sleep_ms(120);
        g_int_hold = true;
        reset_usb_boot(0, 0);
        break;
    case 'U':
#if PANEL_UNDER_BOOTLOADER
        printf("\n  rebooting into the panel bootloader (holds until 'X' or an image)\n");
        sleep_ms(120);
        reboot_into_bootloader();
#else
        printf("\n  standalone build: no bootloader\n");
#endif
        break;
    case '?': help(); break;
    default: break;
    }
}

// ── Core 1 main loop ────────────────────────────────────────────────────────
static void core1_main(void) {
    bool     bus_started = false;
    uint32_t next_refresh_ms = 0, next_console_ms = 0, last_tare_ms = 0;

    while (true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        sense_12v_poll(now_ms);
        if (now_ms != last_tare_ms) { last_tare_ms = now_ms; tare_tick(now_ms); }

        // Once 12V arrives: show the ID image, and start bus mode if the DIP
        // is a panel ID. DIP 10 runs the sensor test standalone instead.
        if (g_12v_present && !bus_started) {
            bus_started = true;
            if (g_dip <= 8) {
                rs485_start();
                g_int_enable = true;
            } else if (g_dip == 10) {
                g_base_mode = MODE_TEST;
            }
        }
        if (g_rs485_on) rs485_service();

        if (g_12v_present && led_dma < 0) led_init();
        if (g_12v_present && !g_boot_id_done) {
            g_boot_id_done = true;
            set_mode(MODE_ID, ID_BOOT_MS);
        }
        if (g_mode_end_ms && (int32_t)(now_ms - g_mode_end_ms) >= 0)
            set_mode(g_base_mode, 0);

        // Debug LED: dark in normal play, press indicator in the DIP-10 test
        // (docs/PANEL_CONFIG.md, decided 2026-07-11).
        gpio_put(PIN_DEBUG_LED, g_dip == 10 && g_pressed_mask);

        if (g_12v_present && led_free() &&
            (g_led_dirty || (int32_t)(now_ms - next_refresh_ms) >= 0)) {
            g_led_dirty = false;
            next_refresh_ms = now_ms + LED_REFRESH_MS;
            render(now_ms);
            led_start();
        }

        if ((int32_t)(now_ms - next_console_ms) >= 0) {
            next_console_ms = now_ms + 20;
            console_poll();
        }
    }
}

int main(void) {
    capture_reset_cause();
    compute_build_tag();

    // SENSE_12V: pulls OFF, first. The reset-default pull-down can make 12V
    // read as absent (docs/PANEL_BRINGUP.md).
    gpio_init(PIN_SENSE_12V);
    gpio_set_dir(PIN_SENSE_12V, GPIO_IN);
    gpio_disable_pulls(PIN_SENSE_12V);

    // No external pull-ups on these nets.
    const uint pu[] = {PIN_TERM_SENSE, PIN_DIP_ID0, PIN_DIP_ID1, PIN_DIP_ID2, PIN_DIP_ID3};
    for (uint i = 0; i < count_of(pu); i++) {
        gpio_init(pu[i]); gpio_set_dir(pu[i], GPIO_IN); gpio_pull_up(pu[i]);
    }

    gpio_init(PIN_DEBUG_LED); gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);
    gpio_put(PIN_DEBUG_LED, 0);
    gpio_init(PIN_RS485_DE); gpio_set_dir(PIN_RS485_DE, GPIO_OUT);
    gpio_put(PIN_RS485_DE, 0);
    int_out_init();

    adc_init();
    for (int i = 0; i < NUM_FSR; i++) adc_gpio_init(FSR_GPIO[i]);
    config_init_defaults();

    sleep_us(100);   // let the DIP pull-ups settle
    g_dip = dip_read();

    stdio_init_all();
    multicore_launch_core1(core1_main);
    fsr_loop();
}
