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
static void int_out_init(void) {
    gpio_init(PIN_INT_OUT);
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

static void int_test(void) {
    printf("  pulsing INT_OUT low 5 times, 200ms on / 800ms off.\n"
           "  With the master present this must land on the header silkscreened for\n"
           "  this panel's position. Standalone, meter GPIO22: ~0V asserted, and\n"
           "  FLOATING (not 3.3V) when released — pushing it high is the bug.\n");
    for (int i = 0; i < 5; i++) {
        int_out_assert();  gpio_put(PIN_DEBUG_LED, 1); sleep_ms(200);
        int_out_release(); gpio_put(PIN_DEBUG_LED, 0); sleep_ms(800);
    }
    printf("  done, released (hi-Z).\n");
}

static void help(void) {
    printf("\n  s  status snapshot          f  stream FSR values\n"
           "  d  watch DIP switch         i  pulse INT_OUT\n"
           "  b  blink debug LED          l  LED test (needs 12V)\n"
           "  p  SENSE_12V pull probe     w  flash write + capacity test\n"
           "  v  re-print banner          ?  this help\n");
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

        int c = getchar_timeout_us(1000);
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
            case '?': help();              break;
            default: break;
        }
    }
}
