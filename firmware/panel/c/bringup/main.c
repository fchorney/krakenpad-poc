// Panel bring-up firmware — dual-panel rev 1 (carrier 2xx + brain 3xx).
//
// This is NOT the gameplay firmware. It exists to prove a freshly assembled
// board works, in the order the hardware allows things to be proven:
//
//   Stage 1  brain alone, USB power    — flash, rails, sense-low, pull config
//   Stage 2  + carrier + 12V, USB kept — power mux, DIP, term, FSRs, INT, LEDs
//   Stage 3  + master + a 2nd panel    — RS-485 (not exercised here; needs peers)
//
// Procedure and the reasoning behind each check: docs/PANEL_BRINGUP.md
// As-built pin map (netlist-derived, differs from ../main.c): docs/DUAL_PANEL.md
//
// Talks over USB CDC. Single core on purpose — nothing here is timing-critical
// and a second core only adds ways for a bring-up tool to lie to you.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/bootrom.h"
#include "ws2812.pio.h"

// ── As-built pin map (docs/DUAL_PANEL.md → "RP2040 (U306) GPIO map") ─────────
#define PIN_RS485_TX    0
#define PIN_RS485_RX    1
#define PIN_RS485_DE    4    // DE + ~RE tied; HIGH = transmit. Also TP306.
#define PIN_TERM_SENSE  10   // LOW = terminated (SW202 pole B, closes to GND)
#define PIN_LED_DATA    11   // → U301 shifter → R301 330R → carrier
#define PIN_DEBUG_LED   16   // carrier D202/R204
#define PIN_SENSE_12V   17   // R313 100k / R314 33k divider. DIGITAL, not ADC.
#define PIN_DIP_ID3     18   // NOTE the reversal: GPIO18 is bit 3, GPIO21 bit 0
#define PIN_DIP_ID2     19
#define PIN_DIP_ID1     20
#define PIN_DIP_ID0     21
#define PIN_INT_OUT     22   // emulated open-drain, sole gameplay press path

#define NUM_FSR   4
#define NUM_LEDS  25

// GPIO26..29 = ADC0..3. Names are the physical panel edge each lands on; the
// carrier connector for each is listed so a swapped pair is identifiable.
static const uint  FSR_GPIO[NUM_FSR] = {26, 27, 28, 29};
static const char *FSR_NAME[NUM_FSR] = {"South", "West", "North", "East"};
static const char *FSR_CONN[NUM_FSR] = {"J202",  "J201", "J206",  "J203"};

// ── Defaults carried forward from the breadboard firmware ───────────────────
// Bench-validated on the prototype; re-confirm each on real hardware before the
// gameplay firmware inherits them.
#define FSR_PRESS_THRESHOLD    500   // 12-bit counts; resting ~100-115, press ~3900
#define FSR_RELEASE_THRESHOLD  400   // hysteresis, stops chatter at the threshold
#define FSR_FLOAT_THRESHOLD    187   // resting average above this = suspicious

// SENSE_12V filter. Asymmetric on purpose: slow to trust the rail, fast to give
// up on it. The pin's whole job is deciding when it is safe to drive WS2815
// DIN pins, so a sagging rail must stop LED output rather than flap.
#define SENSE_POLL_HZ          1000
#define SENSE_RISE_STABLE_MS   30
#define SENSE_FALL_STABLE_MS   2

// ── 12V sense state ─────────────────────────────────────────────────────────
static volatile bool     g_12v_present   = false;
static volatile uint32_t g_12v_edges     = 0;
static volatile bool     g_12v_edge_flag = false;   // set by timer, printed by main
static volatile uint32_t g_12v_edge_ms   = 0;

static bool sense_12v_raw(void) { return gpio_get(PIN_SENSE_12V); }

static bool sense_timer_cb(repeating_timer_t *t) {
    (void)t;
    static uint16_t agree = 0;
    bool raw = sense_12v_raw();
    if (raw == g_12v_present) { agree = 0; return true; }
    agree++;
    uint16_t need = raw ? (SENSE_RISE_STABLE_MS * SENSE_POLL_HZ / 1000)
                        : (SENSE_FALL_STABLE_MS * SENSE_POLL_HZ / 1000);
    if (agree >= need) {
        agree = 0;
        g_12v_present   = raw;
        g_12v_edges++;
        g_12v_edge_ms   = to_ms_since_boot(get_absolute_time());
        g_12v_edge_flag = true;
    }
    return true;
}

// ── Flash (U307 W25Q32JV) ───────────────────────────────────────────────────
// BOOTSEL enumerating proves nothing about the flash — the bootrom enters it
// *because* it could not read a valid image, so a dead or unsoldered U307 looks
// exactly like a blank one. These checks are what actually validate the part.

static void flash_jedec_id(uint8_t out[3]) {
    uint8_t tx[4] = {0x9F, 0, 0, 0};
    uint8_t rx[4] = {0};
    uint32_t save = save_and_disable_interrupts();
    flash_do_cmd(tx, rx, 4);          // handles XIP exit/restore itself
    restore_interrupts(save);
    out[0] = rx[1]; out[1] = rx[2]; out[2] = rx[3];
}

static void report_flash_id(void) {
    uint8_t id[3];
    flash_jedec_id(id);
    bool ok = (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x16);  // W25Q32JV
    printf("  flash JEDEC   : %02X %02X %02X   %s\n", id[0], id[1], id[2],
           ok ? "OK (Winbond W25Q32JV, 4MB)"
              : "UNEXPECTED — expected EF 40 16");

    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    printf("  board id      : ");
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) printf("%02X", uid.id[i]);
    printf("   (read from U307 — use it to name this board's log)\n");
}

// Erase/program/verify the TOP sector, then check whether the pattern also
// appears one alias-length down. A 2MB die substituted for the 4MB part wraps
// addresses, so the write lands at 2MB-4KB and shows up in both windows —
// which is the only way a plain write-and-read-back test can be fooled.
#define TEST_SECTOR   (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define ALIAS_SECTOR  (TEST_SECTOR - (2 * 1024 * 1024))

static void flash_write_test(void) {
    static uint8_t page[FLASH_PAGE_SIZE];
    uint32_t seed = to_ms_since_boot(get_absolute_time());
    for (uint i = 0; i < FLASH_PAGE_SIZE; i++) page[i] = (uint8_t)(seed + i * 31u);

    printf("  erasing + programming 0x%06X (top 4KB, unused)...\n", TEST_SECTOR);
    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(TEST_SECTOR, FLASH_SECTOR_SIZE);
    flash_range_program(TEST_SECTOR, page, FLASH_PAGE_SIZE);
    restore_interrupts(save);

    const uint8_t *at_top   = (const uint8_t *)(XIP_BASE + TEST_SECTOR);
    const uint8_t *at_alias = (const uint8_t *)(XIP_BASE + ALIAS_SECTOR);
    bool verify  = (memcmp(at_top, page, FLASH_PAGE_SIZE) == 0);
    bool aliased = (memcmp(at_alias, page, FLASH_PAGE_SIZE) == 0);

    printf("  write/verify  : %s\n", verify ? "PASS" : "FAIL — U307 does not program");
    printf("  capacity      : %s\n",
           aliased ? "FAIL — pattern echoes at 0x1FF000, die is 2MB not 4MB"
                   : "PASS — no aliasing, full 4MB addressable");
}

// ── SENSE_12V pull-configuration probe ──────────────────────────────────────
// The RP2040 comes out of reset with the pad pull-DOWN enabled, and that is the
// wrong default here: the divider is a 2.98V source behind 24.8k, so a 50k
// internal pull-down drags the pin to ~1.99V — under VIH, i.e. 12V present
// reading as absent. Firmware must disable pulls. This prints what the silicon
// on THIS board actually does under each configuration, so the margin is a
// measured fact rather than my arithmetic.
static void sense_pull_probe(void) {
    printf("  SENSE_12V under each pad pull configuration:\n");

    gpio_disable_pulls(PIN_SENSE_12V);            sleep_ms(5);
    bool none = gpio_get(PIN_SENSE_12V);
    gpio_pull_down(PIN_SENSE_12V);                sleep_ms(5);
    bool down = gpio_get(PIN_SENSE_12V);
    gpio_pull_up(PIN_SENSE_12V);                  sleep_ms(5);
    bool up   = gpio_get(PIN_SENSE_12V);
    gpio_disable_pulls(PIN_SENSE_12V);            sleep_ms(5);

    printf("    pulls off   : %d   <- the configuration firmware must use\n", none);
    printf("    pull-down   : %d\n", down);
    printf("    pull-up     : %d\n", up);
    if (none && !down)
        printf("    ^ CONFIRMED: the reset-default pull-down WOULD have masked 12V.\n"
               "      Record this. It is the reason gpio_disable_pulls(17) is mandatory.\n");
    if (none && down)
        printf("    ^ this board's pull-down is weak enough to still read high.\n"
               "      Do not relax the rule — it is silicon lottery, not margin.\n");
}

// ── INT_OUT: emulated open-drain ────────────────────────────────────────────
// The RP2040 has no true open-drain mode. Assert = drive LOW; release = revert
// to input (hi-Z) and let the master's 10k pull-up do the work. Driving HIGH
// would fight that pull-up and break the documented safe-failure behaviour
// (disconnected wire reads HIGH = not pressed). Never call gpio_put(22, 1).
// gpio_disable_pulls() is MANDATORY and was missing until 2026-09-07. RP2040
// pads reset with the pull-DOWN enabled (PADS_BANK0_GPIOn_PDE_RESET = 1) and
// gpio_init() does not touch pulls — it only sets direction, level and function.
// So "released" was hi-Z *plus a ~45k pull-down*, which fights the master's 10k
// pull-up: measured 2.7V instead of 3.3V on board DE6558A69754442F. Still above
// the Teensy's VIH, so it would have worked — with 0.4V of noise margin on the
// sole gameplay input path instead of 1.0V. This is the GPIO17 trap on a second
// pin, and it is invisible without an external pull-up on the bench.
static void int_out_init(void) {
    gpio_init(PIN_INT_OUT);
    gpio_disable_pulls(PIN_INT_OUT);       // released must be TRUE hi-Z
    gpio_put(PIN_INT_OUT, 0);              // latch low, ready for the dir flip
    gpio_set_dir(PIN_INT_OUT, GPIO_IN);    // released = hi-Z
}
static void int_out_assert(void)  { gpio_set_dir(PIN_INT_OUT, GPIO_OUT); }
static void int_out_release(void) { gpio_set_dir(PIN_INT_OUT, GPIO_IN);  }

// ── DIP + termination ───────────────────────────────────────────────────────
// No board resistors on either net — internal pull-ups are load-bearing.
// Switch closes to GND, so a CLOSED (ON) position reads 0. Panel ID 0 therefore
// means all four switches ON, and an untouched all-OFF switch reads 15.
static uint8_t dip_read(void) {
    return (uint8_t)((gpio_get(PIN_DIP_ID3) << 3) |
                     (gpio_get(PIN_DIP_ID2) << 2) |
                     (gpio_get(PIN_DIP_ID1) << 1) |
                      gpio_get(PIN_DIP_ID0));
}

static const char *dip_meaning(uint8_t v) {
    switch (v) {
        case 9:  return "diag: LED check";
        case 10: return "diag: sensor pressure test";
        case 11: return "diag: standalone";
        case 12: return "diag: raw ADC stream";
        case 13: return "diag: factory reset";
        case 14: return "reserved";
        case 15: return "reserved (also what an unmated brain reads)";
        default: return "panel ID";
    }
}

static uint16_t fsr_read(int ch) {
    adc_select_input(ch);
    return adc_read() & 0x0FFF;
}

// ── LED test — gated on SENSE_12V ───────────────────────────────────────────
// U301 runs from +5VDC, which is live from VBUS alone, so on USB-only power it
// will happily drive LED_DATA_5V into WS2815 DIN pins whose 12V rail is dead,
// forward-biasing their input protection. R301 (330R) keeps that non-destructive
// but it is not a valid test either. This gate is what GPIO17 is FOR.
static PIO  led_pio = pio0;
static uint led_sm;
static bool led_ready = false;

static void led_init(void) {
    uint offset = pio_add_program(led_pio, &ws2812_program);
    led_sm = pio_claim_unused_sm(led_pio, true);
    ws2812_program_init(led_pio, led_sm, offset, PIN_LED_DATA, 800000.0f, false);
    led_ready = true;
}

static void led_fill(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    for (int i = 0; i < NUM_LEDS; i++) pio_sm_put_blocking(led_pio, led_sm, grb << 8u);
    sleep_us(300);   // WS2815 reset is ~280us, longer than WS2812B's 50us
}

// Render a 75-byte 'L' payload: 25 RGB triplets, serpentine order (the mapping
// lives in the master; the panel just paints what it is handed). Called from the
// main loop on a throttle, never from the RS-485 reply path — 25 blocking PIO
// writes at 800kHz is ~750us and the master sends 'L' at 60Hz per panel.
static void led_write_frame(const uint8_t *rgb, int n) {
    if (n > NUM_LEDS) n = NUM_LEDS;
    for (int i = 0; i < n; i++) {
        uint32_t grb = ((uint32_t)rgb[i * 3 + 1] << 16) |   // G
                       ((uint32_t)rgb[i * 3 + 0] << 8)  |   // R
                        (uint32_t)rgb[i * 3 + 2];           // B
        pio_sm_put_blocking(led_pio, led_sm, grb << 8u);
    }
    sleep_us(300);
}

static void led_test(void) {
    if (!g_12v_present) {
        printf("  REFUSED — SENSE_12V is low. The WS2815s have no 12V rail; driving\n"
               "  their DIN pins now proves nothing and forward-biases their inputs.\n");
        return;
    }
    if (!led_ready) led_init();
    // Dim on purpose: full white is 0.44A/panel and the point is presence, not
    // brightness. Red draws exactly as much as white on WS2815 (dies in series)
    // so the colour sequence costs nothing extra.
    const uint8_t v = 32;
    printf("  red...\n");   led_fill(v, 0, 0); sleep_ms(700);
    printf("  green...\n"); led_fill(0, v, 0); sleep_ms(700);
    printf("  blue...\n");  led_fill(0, 0, v); sleep_ms(700);
    printf("  white — count them, all 25 should be lit...\n");
    led_fill(v, v, v); sleep_ms(2500);
    led_fill(0, 0, 0);
    printf("  done. Any dark or wrong-colour pixel localises the break: the chain\n"
           "  runs serpentine and the row of 3 is intentionally 180 degrees out.\n");
}

// ── Status ──────────────────────────────────────────────────────────────────
static void print_status(void) {
    uint8_t dip = dip_read();
    bool term    = !gpio_get(PIN_TERM_SENSE);   // LOW = terminated
    bool mated   = (dip != 15) || term;         // best-effort hint, not proof

    printf("\n--- status @ %lu ms ---\n", (unsigned long)to_ms_since_boot(get_absolute_time()));
    printf("  SENSE_12V     : %s  (raw pin %d, %lu edge(s) since boot)\n",
           g_12v_present ? "12V PRESENT" : "12V ABSENT",
           sense_12v_raw(), (unsigned long)g_12v_edges);
    printf("  DIP           : %u  (0b%d%d%d%d, %s)\n", dip,
           (dip >> 3) & 1, (dip >> 2) & 1, (dip >> 1) & 1, dip & 1, dip_meaning(dip));
    printf("  TERM_SENSE    : %s\n", term ? "TERMINATED (120R across the pair)"
                                          : "not terminated");
    printf("  carrier       : %s\n", mated ? "looks mated" :
           "looks UNMATED (DIP 15 + unterminated = brain alone, or all switches off)");
    printf("  FSR raw       :");
    for (int i = 0; i < NUM_FSR; i++) printf(" %s=%4u", FSR_NAME[i], fsr_read(i));
    printf("\n");
    for (int i = 0; i < NUM_FSR; i++) {
        uint16_t v = fsr_read(i);
        if (v > FSR_FLOAT_THRESHOLD)
            printf("  ! %s (%s) rests at %u, above the %u float threshold\n",
                   FSR_NAME[i], FSR_CONN[i], v, FSR_FLOAT_THRESHOLD);
    }
}

static void fsr_stream(void) {
    printf("  streaming — press each edge in turn and check the NAME that moves\n"
           "  matches the edge you pressed. Any key stops.\n");
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
        printf("\r  ");
        for (int i = 0; i < NUM_FSR; i++) {
            uint16_t v = fsr_read(i);
            printf("%s %4u %s  ", FSR_NAME[i], v,
                   v >= FSR_PRESS_THRESHOLD ? "[PRESS]" : "       ");
        }
        fflush(stdout);
        sleep_ms(50);
    }
    printf("\n  stopped.\n");
}

static void dip_watch(void) {
    printf("  toggle one position at a time. GPIO18 is bit 3 and GPIO21 is bit 0 —\n"
           "  the order is REVERSED relative to SW201's 1-4 labelling, and a closed\n"
           "  switch reads 0. This is the one place to prove that physically.\n"
           "  Any key stops.\n");
    uint8_t last = 0xFF;
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
        uint8_t d = dip_read();
        if (d != last) {
            last = d;
            printf("  DIP = %2u  0b%d%d%d%d  (%s)\n", d,
                   (d >> 3) & 1, (d >> 2) & 1, (d >> 1) & 1, d & 1, dip_meaning(d));
        }
        sleep_ms(20);
    }
    printf("  stopped.\n");
}

// Two phases, because they need different instruments.
//
// The 5 fast pulses are for the master end of the wire (and for a scope): they
// show edges landing on the right JST XH header. They are USELESS on a handheld
// DMM — 200ms low / 800ms high is a 1Hz square wave, and a meter that cannot
// track it reports the time-average, 0.8 * 3.3 = 2.64V, which looks like a
// broken pin but is a correct one seen through a slow instrument. (Measured on
// board DE6558A69754442F: 2.7V "resting", dipping to 1.3V. Both artefacts.)
//
// So the second phase holds each state still for DWELL_MS. That is the phase to
// meter. Standalone it needs an external pull-up (10k from TP301 to J302 pin 6)
// or both states read ~0V through the meter's own impedance and prove nothing.
#define INT_DWELL_MS 8000

static void int_test(void) {
    printf("  Phase 1: pulsing INT_OUT low 5 times, 200ms on / 800ms off.\n"
           "  For the MASTER end or a scope — with the master present these edges\n"
           "  must land on the header silkscreened for this panel's position.\n"
           "  Do NOT try to read this phase on a handheld meter; it will show you\n"
           "  the ~2.6V duty-cycle average and look like a fault.\n");
    for (int i = 0; i < 5; i++) {
        int_out_assert();  gpio_put(PIN_DEBUG_LED, 1); sleep_ms(200);
        int_out_release(); gpio_put(PIN_DEBUG_LED, 0); sleep_ms(800);
    }

    printf("\n  Phase 2: steady dwells, %d s each — THIS is the one to meter.\n"
           "  Standalone you need 10k from TP301 (+3.3VDC) to J302 pin 6, else\n"
           "  both readings are ~0V and distinguish nothing.\n", INT_DWELL_MS / 1000);

    printf("\n  >>> ASSERTED (driven LOW) — read the meter now. Expect ~0V.\n");
    int_out_assert(); gpio_put(PIN_DEBUG_LED, 1);
    sleep_ms(INT_DWELL_MS);

    printf("  >>> RELEASED (hi-Z) — read again. Expect ~3.3V via the pull-up.\n"
           "      If this reads ~0V the pin is stuck driven low. If it reads 3.3V\n"
           "      with the pull-up REMOVED, firmware is driving it high — the bug\n"
           "      this test exists to catch.\n");
    int_out_release(); gpio_put(PIN_DEBUG_LED, 0);
    sleep_ms(INT_DWELL_MS);

    printf("\n  done, released (hi-Z).\n");
}

// ── RS-485 responder (stage 3) ──────────────────────────────────────────────
// A minimal peer for docs/RS485_PROTOCOL.md — enough to make the master's bus
// and its slot<->ID self-test real. NOT the gameplay firmware: no animations,
// no flash config, no persistence. ../main.c still needs its wholesale port.
//
// Frame, both directions: 0x55 | cmd | addr | len | payload[len] | crc8
// CRC-8 poly 0x07 init 0x00, over cmd..payload.
//
// Answers frames addressed to this panel's DIP ID, or to 0xFF broadcast:
//   'F' -> 'f'   4 x uint16 LE raw ADC, then a pressed bitmask
//   'I' -> 'i'   ack FIRST, then pull INT low for payload[0] ms
//   'C' -> 'c'   set thresholds (RAM only), echo what was applied
//   'L'          LED frame — counted; displayed only if 'E' is on, see below
//
// ⚠ THREE things this code must not do. Two of them already bit this project.
//
//   1. NEVER printf on the reply path. A USB CDC write takes ~ms; the master
//      polls every 5 ms and expects a reply in ~150 us. So the responder is
//      SILENT while running and stats print only on demand ('T'). "One dropped
//      reply per second" that turns out to be your own logging is exactly the
//      kind of lie a bring-up tool must not tell.
//   2. RX must be interrupt-driven. The main loop's getchar_timeout_us(1000)
//      blocks a full millisecond — 100 byte times at 1 Mbps against a 32-byte
//      FIFO. Polling the UART from that loop drops frames. The IRQ fills a
//      ring; the loop parses it. The loop also stops blocking while 'R' is on.
//   3. DE must be released on the PL011's BUSY flag, never on FIFO-empty.
//      TX-FIFO-empty does NOT mean the shift register is empty: release there
//      and the last byte is truncated. uart_tx_wait_blocking() does wait on
//      BUSY, which is correct — but a character time of slack is added anyway,
//      because releasing early costs a byte and releasing late costs nothing
//      here (the master paces polls 5 ms apart).
#define RS485_BAUD      1000000u
#define RS485_SYNC      0x55
#define RS485_BCAST     0xFF
#define RS485_MAX_PAY   80
#define RS_RING         2048   // must outlast a 50ms 'I' pulse at ~42 kB/s

static bool g_rs485_on   = false;
static bool g_rs485_init = false;
static bool g_led_echo   = false;   // 'E'; painting is throttled in the main loop
static uint8_t  g_led_buf[3 * NUM_LEDS];
static uint8_t  g_led_len   = 0;
static bool     g_led_dirty = false;
static bool g_auto_int   = false;   // 'A'; drive INT_OUT from FSR thresholds
static bool g_int_asserted = false;

static uint16_t g_press_th[NUM_FSR];
static uint16_t g_rel_th[NUM_FSR];
static bool     g_pressed[NUM_FSR];

static volatile uint8_t  rs_ring[RS_RING];
static volatile uint16_t rs_head = 0, rs_tail = 0;
static volatile uint32_t rs_overruns = 0;
// Forensics for CRC failures. A rate this low (single digits in ~10^5 frames)
// is not "noise" in any useful sense — it has a cause, and the cheapest way to
// find it is to keep the actual bytes. The prime suspect on a half-duplex bus is
// TURNAROUND: when either end drops DE the transceiver's receiver re-enables
// while the line is still settling, and a false start bit becomes a spurious
// byte that desyncs the parser mid-frame. If that is what is happening, the
// failures cluster within a few hundred us of OUR OWN last transmission, and the
// captured bytes show a frame with one extra byte in front. Scattered failures
// with no TX correlation mean something else: reflections, or a real bit error.
#define RS_FAIL_KEEP 4
typedef struct {
    uint8_t  raw[16];      // sync + header + as much payload as fits
    uint8_t  rawlen;       // bytes captured
    uint8_t  framelen;     // bytes the frame claimed (4 + len + 1)
    uint8_t  got, want;    // received vs computed CRC
    uint32_t since_tx_us;  // gap from our last transmission
} rs_fail_t;
static rs_fail_t rs_fails[RS_FAIL_KEEP];
static volatile uint32_t rs_uart_errs = 0;
static volatile uint32_t rs_turnaround = 0;
static volatile bool     rs_resync = false;
static uint8_t   rs_fail_idx = 0;
static uint32_t  rs_last_tx_us = 0;

static uint32_t rs_frames = 0, rs_crc_errs = 0, rs_polls = 0,
                rs_led = 0, rs_ident = 0, rs_cfg = 0, rs_notmine = 0;

static void rs485_rx_irq(void) {
    while (uart_is_readable(uart0)) {
        // The PL011 reports per-byte errors in the TOP bits of DR. Masking them
        // off and keeping only the data — which this did at first — throws away
        // the one signal that says "bytes were lost here". OE in particular means
        // the hardware FIFO overflowed and the stream now has a hole in it, so
        // whatever frame is in flight is garbage no matter what its CRC says.
        uint32_t dr = uart_get_hw(uart0)->dr;
        if (dr & 0xF00u) {          // FE | PE | BE | OE
            // Separate the structural artifact from a real fault. Anything
            // within 200us of our own DE release is the turnaround glitch above;
            // counting it as an error buries the ones that matter. Both resync —
            // a hole in the stream invalidates any frame in flight either way.
            if ((time_us_32() - rs_last_tx_us) < 200u) rs_turnaround++;
            else                                       rs_uart_errs++;
            rs_resync = true;
        }
        uint8_t c = (uint8_t)(dr & 0xFF);
        uint16_t next = (uint16_t)((rs_head + 1u) % RS_RING);
        // ⚠ MUST keep draining the FIFO even when the ring is full. Returning
        // here with bytes still pending leaves the RX interrupt asserted, so it
        // re-fires immediately, forever — the main loop never runs, the ring
        // never drains, and the board livelocks with USB dead. It would have
        // fired on the first 'I': that handler blocks up to 50ms pulsing INT,
        // which at ~42 kB/s of bus traffic is thousands of bytes into a ring
        // this size. Drop the BYTE, never the drain.
        if (next == rs_tail) { rs_overruns++; continue; }
        rs_ring[rs_head] = c;
        rs_head = next;
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
    busy_wait_us(2);                      // driver enabled before the start bit
    uart_write_blocking(uart0, hdr, 4);
    if (len) uart_write_blocking(uart0, pay, len);
    uart_write_blocking(uart0, &crc, 1);
    uart_tx_wait_blocking(uart0);         // BUSY flag, not FIFO-empty — see (3)
    busy_wait_us(12);                     // ~1 char at 1 Mbps of slack
    gpio_put(PIN_RS485_DE, 0);            // back to receive
    rs_last_tx_us = time_us_32();
}

// One source of truth for press state: the 'F' reply and the auto-INT loop must
// never disagree about whether a channel is pressed, or the telemetry the master
// shows and the INT edge it acts on describe different realities.
static uint8_t fsr_update(uint16_t raw_out[NUM_FSR]) {
    uint8_t mask = 0;
    for (int i = 0; i < NUM_FSR; i++) {
        uint16_t v = fsr_read(i);
        if (raw_out) raw_out[i] = v;
        if      (v >= g_press_th[i]) g_pressed[i] = true;
        else if (v <= g_rel_th[i])   g_pressed[i] = false;
        if (g_pressed[i]) mask |= (uint8_t)(1u << i);
    }
    return mask;
}

static void rs485_handle(uint8_t cmd, uint8_t addr, const uint8_t *pay, uint8_t len) {
    uint8_t me = dip_read();
    if (addr != me && addr != RS485_BCAST) { rs_notmine++; return; }

    switch (cmd) {
    case 'F': {
        uint16_t raw[NUM_FSR];
        uint8_t r[9], mask = fsr_update(raw);
        for (int i = 0; i < NUM_FSR; i++) {
            r[i * 2]     = (uint8_t)(raw[i] & 0xFF);
            r[i * 2 + 1] = (uint8_t)(raw[i] >> 8);
        }
        r[8] = mask;
        rs485_send('f', me, r, 9);
        rs_polls++;
        break;
    }
    case 'I': {
        uint16_t ms = len ? pay[0] : 2;
        if (ms < 1)  ms = 1;
        if (ms > 50) ms = 50;
        // Ack FIRST, then pulse: the master needs an explicit "expect an edge in
        // the next N ms" window so correlating it needs no guesswork. rs485_send
        // has already dropped DE, but give the bus a moment to settle before
        // loading it with an INT edge.
        rs485_send('i', me, NULL, 0);
        busy_wait_us(50);
        int_out_assert();
        sleep_ms(ms);
        int_out_release();
        rs_ident++;
        break;
    }
    case 'C':
        if (len >= 5) {
            uint8_t  ch = pay[0];
            uint16_t p  = (uint16_t)(pay[1] | (pay[2] << 8));
            uint16_t rl = (uint16_t)(pay[3] | (pay[4] << 8));
            if (ch == 0xFF)
                for (int i = 0; i < NUM_FSR; i++) { g_press_th[i] = p; g_rel_th[i] = rl; }
            else if (ch < NUM_FSR) { g_press_th[ch] = p; g_rel_th[ch] = rl; }
            rs485_send('c', me, pay, len);
        }
        rs_cfg++;
        break;
    case 'L':
        rs_led++;
        // Buffer only — painting 25 pixels is ~750us of blocking PIO writes and
        // the master sends 'L' at 60Hz per panel, so doing it here would stall
        // the reply path and make a healthy bus look lossy. The main loop paints
        // it on a throttle instead, which is why 'E' is safe to leave on.
        if (g_led_echo && len >= 3) {
            memcpy(g_led_buf, pay, len < sizeof(g_led_buf) ? len : sizeof(g_led_buf));
            g_led_len   = len;
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
    static uint8_t dbg[16], dbglen = 0;

    if (rs_resync) { rs_resync = false; st = W_SYNC; dbglen = 0; }

    while (rs_tail != rs_head) {
        uint8_t c = rs_ring[rs_tail];
        rs_tail = (uint16_t)((rs_tail + 1u) % RS_RING);

        if (st == W_SYNC) { if (c == RS485_SYNC) { dbglen = 0; } }
        if (dbglen < sizeof(dbg)) dbg[dbglen++] = c;

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
            if (c == crc) { rs_frames++; rs485_handle(cmd, addr, pay, len); }
            else {
                rs_crc_errs++;
                rs_fail_t *f = &rs_fails[rs_fail_idx % RS_FAIL_KEEP];
                rs_fail_idx++;
                f->rawlen   = dbglen;
                f->framelen = (uint8_t)(4 + len + 1);
                f->got = c; f->want = crc;
                f->since_tx_us = time_us_32() - rs_last_tx_us;
                memcpy(f->raw, dbg, dbglen);
            }
            st = W_SYNC;
            break;
        }
    }
}

static void rs485_dump_fails(void) {
    printf("\n  CRC failures captured this run: %lu\n", (unsigned long)rs_crc_errs);
    if (rs_fail_idx == 0) {
        printf("    none — nothing has failed CRC since the responder was enabled.\n");
        return;
    }
    uint8_t n = rs_fail_idx < RS_FAIL_KEEP ? rs_fail_idx : RS_FAIL_KEEP;
    for (uint8_t k = 0; k < n; k++) {
        rs_fail_t *f = &rs_fails[(uint8_t)((rs_fail_idx - 1 - k) % RS_FAIL_KEEP)];
        printf("    [-%u] crc got %02X want %02X, frame claimed %u bytes,"
               " %lu us after our own last TX\n",
               k, f->got, f->want, f->framelen, (unsigned long)f->since_tx_us);
        printf("         bytes:");
        for (uint8_t i = 0; i < f->rawlen; i++) printf(" %02X", f->raw[i]);
        printf("%s\n", f->rawlen >= sizeof(((rs_fail_t *)0)->raw) ? " ..." : "");
        if (f->since_tx_us < 500)
            printf("         ^^ within 500us of our own transmission — the signature\n"
                   "            of BUS TURNAROUND, not of a noisy cable.\n");
    }
    printf("\n  A good frame starts 55 <cmd> <addr> <len>. Read the first bytes:\n"
           "    - an extra byte BEFORE the 55        -> spurious start bit, turnaround\n"
           "    - 55 present but cmd/len implausible -> parser resynced mid-frame on a\n"
           "                                            0x55 that was really payload\n"
           "    - everything plausible, one bit off  -> a real bit error on the wire\n");
}

static void rs485_init(void) {
    uart_init(uart0, RS485_BAUD);
    gpio_set_function(PIN_RS485_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RS485_RX, GPIO_FUNC_UART);
    // ⚠ Idle-bias the RX pin. While WE transmit, DE (tied to ~RE) disables the
    // THVD1450's receiver and RO goes high-impedance, so this pin floats. When
    // DE drops and RO drives again, the transition is seen as a false start bit
    // and the UART reports a framing error — measured at EXACTLY one per
    // transmission (2522 errors against 2521 replies over 90,756 frames). It
    // corrupts nothing, because it lands in the idle gap right after our own
    // reply, but it is noise in a counter that should mean something. An
    // internal pull-up holds the line at idle-mark through the Hi-Z window and
    // removes the glitch at its source, with no board change.
    gpio_pull_up(PIN_RS485_RX);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    uart_set_hw_flow(uart0, false, false);
    gpio_put(PIN_RS485_DE, 0);
    irq_set_exclusive_handler(UART0_IRQ, rs485_rx_irq);
    irq_set_enabled(UART0_IRQ, true);
    // NOTE: RX interrupts are enabled/disabled by the 'R' toggle, not here. With
    // them left on while the responder is off, the ISR keeps filling a ring that
    // nothing drains: it fills in ~46ms and then every byte after that counts as
    // an overrun forever. That produced 8.7M "overruns" on a bus that was in
    // fact healthy, which is a counter actively lying about the hardware.
    for (int i = 0; i < NUM_FSR; i++) {
        g_press_th[i] = FSR_PRESS_THRESHOLD;
        g_rel_th[i]   = FSR_RELEASE_THRESHOLD;
        g_pressed[i]  = false;
    }
    g_rs485_init = true;
}

static void rs485_stats(void) {
    uint8_t me = dip_read();
    printf("\n  RS-485 responder: %s\n", g_rs485_on ? "ON" : "off");
    printf("  my address    : %u  (%s)\n", me, dip_meaning(me));
    if (me > 8)
        printf("  ** THE MASTER WILL NEVER ADDRESS THIS PANEL — it polls IDs 0-8.\n"
               "     Set the DIP to a panel ID. Remember CLOSED = 0, so ID 0 is\n"
               "     all four switches ON and an untouched all-OFF switch is 15.\n");
    printf("  frames ok     : %lu\n", (unsigned long)rs_frames);
    printf("  crc errors    : %lu\n", (unsigned long)rs_crc_errs);
    printf("  rx overruns   : %lu   (ring full — main loop starved)\n",
           (unsigned long)rs_overruns);
    printf("  uart errors   : %lu   (FIFO overrun / framing — bytes lost on the wire)\n",
           (unsigned long)rs_uart_errs);
    printf("  turnaround    : %lu   (glitch within 200us of our own TX — benign)\n",
           (unsigned long)rs_turnaround);
    printf("  'F' answered  : %lu\n", (unsigned long)rs_polls);
    printf("  'L' received  : %lu   (echo to LEDs: %s)\n",
           (unsigned long)rs_led, g_led_echo ? "ON" : "off");
    printf("  'I' answered  : %lu\n", (unsigned long)rs_ident);
    printf("  'C' answered  : %lu\n", (unsigned long)rs_cfg);
    printf("  not for me    : %lu   (other panels' traffic — normal on a shared bus)\n",
           (unsigned long)rs_notmine);
    if (rs_frames == 0 && rs_crc_errs == 0)
        printf("  => NOTHING RECEIVED AT ALL. Check A/B not swapped, termination at\n"
               "     both ends only, a shared ground, and U308's solder joints.\n");
    else if (rs_frames == 0 && rs_crc_errs > 0)
        printf("  => Bytes arrive but no frame validates: bus is alive, so suspect\n"
               "     baud, A/B polarity, or reflections from missing termination.\n");
}

static void help(void) {
    printf("\n  s  status snapshot          f  stream FSR values\n"
           "  d  watch DIP switch         i  pulse INT_OUT\n"
           "  b  blink debug LED          l  LED test (needs 12V)\n"
           "  p  SENSE_12V pull probe     w  flash write + capacity test\n"
           "  v  re-print banner          ?  this help\n"
           "\n  stage 3 (RS-485, needs the master):\n"
           "  R  responder on/off         T  responder counters\n"
           "  X  dump the last CRC failures (raw bytes + TX correlation)\n"
           "  E  echo 'L' frames to LEDs   A  drive INT_OUT from FSR thresholds\n"
           "  B  reboot into BOOTSEL for reflashing\n");
}

// ── Stage 1 self-test ───────────────────────────────────────────────────────
static void banner(void) {
    printf("\n========================================================\n");
    printf(" dual-panel bring-up  (built %s %s)\n", __DATE__, __TIME__);
    printf("========================================================\n");
    printf("  sysclk        : %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));
    report_flash_id();
    printf("  SENSE_12V     : %s\n", sense_12v_raw() ? "HIGH" : "LOW");
    if (sense_12v_raw())
        printf("    (expected LOW on USB-only power. HIGH here with no 12V applied\n"
               "     means a bridge to +3.3V or a wrong R313 — stop and check.)\n");
    printf("  RS485_DE      : driven LOW (receive). Rest state before firmware runs\n"
           "                  is a separate meter check at TP306 on a BLANK board.\n");
    printf("--------------------------------------------------------\n");
    printf("  '?' for commands. This banner reappearing on its own means the board\n"
           "  RESET — which is exactly what a power-mux glitch looks like.\n");
}

int main(void) {
    stdio_init_all();

    // SENSE_12V: pulls OFF, deliberately and first. See sense_pull_probe().
    gpio_init(PIN_SENSE_12V);
    gpio_set_dir(PIN_SENSE_12V, GPIO_IN);
    gpio_disable_pulls(PIN_SENSE_12V);

    // No external pull-ups exist on these nets. Unmated they read all-ones.
    const uint pu[] = {PIN_TERM_SENSE, PIN_DIP_ID0, PIN_DIP_ID1, PIN_DIP_ID2, PIN_DIP_ID3};
    for (uint i = 0; i < count_of(pu); i++) {
        gpio_init(pu[i]); gpio_set_dir(pu[i], GPIO_IN); gpio_pull_up(pu[i]);
    }

    gpio_init(PIN_DEBUG_LED); gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);
    gpio_put(PIN_DEBUG_LED, 0);

    // Park the transceiver in receive explicitly rather than trusting the pad
    // default, and keep it there — this build never transmits.
    gpio_init(PIN_RS485_DE); gpio_set_dir(PIN_RS485_DE, GPIO_OUT);
    gpio_put(PIN_RS485_DE, 0);

    int_out_init();

    adc_init();
    for (int i = 0; i < NUM_FSR; i++) adc_gpio_init(FSR_GPIO[i]);

    // Seed the filter from the pin so plugging in 12V before USB is not an edge.
    g_12v_present = sense_12v_raw();
    static repeating_timer_t sense_timer;
    add_repeating_timer_us(-1000000 / SENSE_POLL_HZ, sense_timer_cb, NULL, &sense_timer);

    // Give the host a moment to attach, but never block on it.
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++) sleep_ms(100);
    banner();
    help();

    absolute_time_t next_paint = get_absolute_time();
    absolute_time_t next_beat = get_absolute_time();
    bool beat = false;

    while (true) {
        if (g_12v_edge_flag) {
            g_12v_edge_flag = false;
            printf("\n  [%lu ms] SENSE_12V -> %s\n", (unsigned long)g_12v_edge_ms,
                   g_12v_present ? "12V PRESENT" : "12V ABSENT");
        }

        // 1Hz heartbeat: a reset is visible on the board even with no terminal.
        if (absolute_time_diff_us(get_absolute_time(), next_beat) <= 0) {
            beat = !beat;
            gpio_put(PIN_DEBUG_LED, beat);
            next_beat = delayed_by_ms(get_absolute_time(), 500);
        }

        // ⚠ Do NOT block for a millisecond while the responder is live: at
        // 1 Mbps that is 100 byte times against a 32-byte FIFO. The IRQ still
        // catches bytes, but the ring only drains here, so keep the loop tight.
        if (g_rs485_on) rs485_service();

        // Paint the last 'L' frame, throttled and OFF the reply path. led_init()
        // is lazy and 12V-gated for the same reason led_test() is: driving WS2815
        // DIN with their 12V rail dead forward-biases their inputs.
        if (g_led_dirty && g_12v_present &&
            absolute_time_diff_us(get_absolute_time(), next_paint) <= 0) {
            if (!led_ready) led_init();
            led_write_frame(g_led_buf, g_led_len / 3);
            g_led_dirty = false;
            next_paint = delayed_by_ms(get_absolute_time(), 33);   // ~30Hz
        }

        // Auto-INT: drive the REAL gameplay press path from the FSR thresholds,
        // using the same hysteresis state the 'F' reply reports, so telemetry and
        // the INT edge can never disagree. Press any sensor and the master should
        // print "PRESS panel N" — panel ADC -> threshold -> open-drain -> wire ->
        // master ISR, the whole path, with nothing simulated.
        if (g_auto_int) {
            bool pressed = (fsr_update(NULL) != 0);
            if (pressed != g_int_asserted) {
                g_int_asserted = pressed;
                if (pressed) int_out_assert(); else int_out_release();
            }
        }

        int c = getchar_timeout_us(g_rs485_on ? 0 : 1000);
        switch (c) {
            case 's': print_status();      break;
            case 'f': fsr_stream();        break;
            case 'd': dip_watch();         break;
            case 'i': int_test();          break;
            case 'l': led_test();          break;
            case 'p': sense_pull_probe();  break;
            case 'w': flash_write_test();  break;
            case 'v': banner();            break;
            case 'b':
                for (int i = 0; i < 6; i++) {
                    gpio_put(PIN_DEBUG_LED, i & 1); sleep_ms(150);
                }
                printf("  debug LED blinked 3x (carrier D202).\n");
                break;
            case 'R':
                if (!g_rs485_init) rs485_init();
                g_rs485_on = !g_rs485_on;
                if (g_rs485_on) {
                    // Fresh window: counters describe THIS run, not the ring
                    // that backed up while the responder was off.
                    rs_frames = rs_crc_errs = rs_polls = rs_led = 0;
                    rs_ident  = rs_cfg = rs_notmine = 0;
                    rs_overruns = 0;
                    rs_uart_errs = 0;
                    rs_turnaround = 0;
                    rs_fail_idx = 0;
                    rs_head = rs_tail = 0;
                    // ⚠ FLUSH THE HARDWARE FIFO. The UART receives whether or
                    // not its interrupt is enabled, so by the time the responder
                    // is switched on the 32-byte FIFO holds STALE bytes from an
                    // arbitrary earlier moment, with OE long since set. Feeding
                    // those into the ring splices an old fragment onto the live
                    // stream: the result parses as a structurally perfect frame
                    // — right sync, right command, plausible payload — that
                    // fails CRC, because its two halves came from different
                    // frames. That, not bus noise, was every CRC error seen at
                    // the bench; steady-state was 0 in 68,257 frames.
                    while (uart_is_readable(uart0)) (void)uart_get_hw(uart0)->dr;
                    uart_get_hw(uart0)->rsr = 0x0Fu;   // clear OE/BE/PE/FE
                    rs_last_tx_us = time_us_32();
                    rs_resync = true;
                }
                uart_set_irq_enables(uart0, g_rs485_on, false);
                printf("\n  RS-485 responder %s (address %u). Silent while running —\n"
                       "  press 'T' for counters. See docs/PANEL_BRINGUP.md stage 3.\n",
                       g_rs485_on ? "ON" : "OFF", dip_read());
                break;
            case 'T': rs485_stats();       break;
            case 'X': rs485_dump_fails();  break;
            case 'A':
                g_auto_int = !g_auto_int;
                if (!g_auto_int) { int_out_release(); g_int_asserted = false; }
                printf("\n  auto-INT %s — INT_OUT now follows the FSR thresholds"
                       " (press %u / release %u).\n%s",
                       g_auto_int ? "ON" : "off",
                       g_press_th[0] ? g_press_th[0] : FSR_PRESS_THRESHOLD,
                       g_rel_th[0]   ? g_rel_th[0]   : FSR_RELEASE_THRESHOLD,
                       g_auto_int ? "  Stand on a sensor: the master should print"
                                    " PRESS/RELEASE for this panel.\n"
                                  : "  INT_OUT released (hi-Z).\n");
                break;
            case 'B':
                // Reboot into BOOTSEL so reflashing needs no button. Stage 3 is
                // an edit/build/flash loop; SW301 is under the panel platform.
                printf("\n  rebooting into BOOTSEL — drag the .uf2 onto RPI-RP2.\n");
                sleep_ms(120);          // let the CDC write drain first
                reset_usb_boot(0, 0);
                break;
            case 'E':
                g_led_echo = !g_led_echo;
                printf("\n  'L' -> LED echo %s.%s\n", g_led_echo ? "ON" : "off",
                       g_led_echo ? " Painting is throttled to ~30Hz in the main"
                                    " loop, so it does not touch reply timing."
                                    " Needs 12V." : "");
                break;
            case '?': help();              break;
            default: break;
        }
    }
}
