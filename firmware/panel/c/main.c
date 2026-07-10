#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

// ── Pin assignments ───────────────────────────────────────────────────────────
#define DEBUG_PRESS_PIN 3  // push-pull, HIGH on press — drives debug LED only
#define LED_PIN     4
#define INT_PIN     5      // open-drain to master: sink LOW on press, high-Z idle
#define PANEL_ID_PIN 6     // internal pull-down: floating = panel 0, jumper to 3V3 OUT = panel 1

// ── RS-485 (see docs/RS485_PROTOCOL.md) ─────────────────────────────────────
#define RS485_UART      uart0
#define RS485_BAUD      1000000
#define RS485_TX_PIN    0
#define RS485_RX_PIN    1
#define RS485_DE_PIN    2       // DE + ~RE tied: HIGH = transmit, LOW = receive
#define PKT_SYNC        0x55
#define PKT_MAX_PAYLOAD 80
#define LED_FRAME_LEN   75      // 25 LEDs × RGB
#define LED_FALLBACK_MS 100     // no remote frame for this long -> local animation
#define NUM_LEDS    25
#define NUM_FSR     4

// ADC returns 12-bit values (0–4095), unlike MicroPython's read_u16() (0–65535).
// MicroPython threshold of 8000/65535 * 4095 ≈ 500 here.
// Hysteresis: press and release use different thresholds so ADC noise near the
// press point can't chatter the state (was producing ~0.3ms phantom presses).
// These are boot defaults; runtime values live in g_press_thr/g_rel_thr and are
// settable over RS-485 (cmd 'C'). RAM only for now — flash config comes later.
#define FSR_PRESS_THRESHOLD   500
#define FSR_RELEASE_THRESHOLD 400
#define FSR_FLOAT_THRESHOLD   187  // resting avg above this = floating/disconnected
// Persistence filter: a state change must hold for this many consecutive samples
// before it's committed (~0.1ms at the ~100kHz loop rate).
#define FSR_PERSISTENCE     10
// Discard the first ADC conversion after each mux switch to absorb sample-cap
// charge injection (~15 counts of crosstalk from a pressed adjacent channel).
// Halves the sample rate. Set to 0 when the ADC nodes have ~10nF caps to GND —
// the caps fix the same problem in hardware.
#define ADC_DUMMY_READ      0

static const uint FSR_GPIO[NUM_FSR] = {26, 27, 28, 29};

// ── LED patterns ─────────────────────────────────────────────────────────────
//
// Physical layout (serpentine wiring):
// 00--01--02--03
// --06--05--04--
// 07--08--09--10
// --13--12--11--
// 14--15--16--17
// --20--19--18--
// 21--22--23--24

static const uint8_t PATTERN_SERPENTINE[NUM_LEDS] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24
};

// Left-to-right, top-to-bottom physical order (odd rows wired R→L so reversed here)
static const uint8_t PATTERN_READING[NUM_LEDS] = {
     0,  1,  2,  3,   // row 0 — L→R
     6,  5,  4,       // row 1 — wired R→L
     7,  8,  9, 10,   // row 2 — L→R
    13, 12, 11,       // row 3 — wired R→L
    14, 15, 16, 17,   // row 4 — L→R
    20, 19, 18,       // row 5 — wired R→L
    21, 22, 23, 24,   // row 6 — L→R
};

// Panel identity: read once at boot from PANEL_ID_PIN (internal pull-down).
// Bench-test stand-in for the final PCB's 4-position DIP switch (0-8).
static uint8_t g_panel_id = 0;

// ── Shared state (Core 0 writes, Core 1 reads) ───────────────────────────────
static volatile bool     g_pressed          = false;
static volatile uint16_t g_fsr_vals[NUM_FSR] = {0};
static volatile bool     g_active_fsrs[NUM_FSR] = {false};
static bool              g_fsr_pressed[NUM_FSR] = {false};  // per-channel hysteresis state (Core 0 only)
static uint8_t           g_fsr_streak[NUM_FSR]  = {0};      // consecutive samples disagreeing with current state
static volatile uint8_t  g_pressed_mask = 0;                // bit i = channel i pressed (Core 0 writes)
// Runtime thresholds (Core 1 writes on config command, Core 0 reads)
static volatile uint16_t g_press_thr[NUM_FSR] =
    {FSR_PRESS_THRESHOLD, FSR_PRESS_THRESHOLD, FSR_PRESS_THRESHOLD, FSR_PRESS_THRESHOLD};
static volatile uint16_t g_rel_thr[NUM_FSR] =
    {FSR_RELEASE_THRESHOLD, FSR_RELEASE_THRESHOLD, FSR_RELEASE_THRESHOLD, FSR_RELEASE_THRESHOLD};

// ── Helpers ──────────────────────────────────────────────────────────────────
// WS2812B expects GRB order; pack into upper 24 bits for the PIO TX FIFO.
static inline uint32_t rgb_to_grb_word(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
}

static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == 0.0f) { *r = *g = *b = (uint8_t)(v * 255); return; }
    int   i  = (int)(h / 60.0f) % 6;
    float f  = h / 60.0f - (int)(h / 60.0f);
    float p  = v * (1 - s);
    float q  = v * (1 - s * f);
    float t  = v * (1 - s * (1 - f));
    float rv, gv, bv;
    switch (i) {
        case 0: rv=v; gv=t; bv=p; break;
        case 1: rv=q; gv=v; bv=p; break;
        case 2: rv=p; gv=v; bv=t; break;
        case 3: rv=p; gv=q; bv=v; break;
        case 4: rv=t; gv=p; bv=v; break;
        default: rv=v; gv=p; bv=q; break;
    }
    *r = (uint8_t)(rv * 255);
    *g = (uint8_t)(gv * 255);
    *b = (uint8_t)(bv * 255);
}

// ── RS-485: framing + RX (see docs/RS485_PROTOCOL.md) ───────────────────────
static uint8_t crc8_update(uint8_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    return crc;
}

// RX ring buffer: UART IRQ produces, Core 1 loop consumes
static volatile uint8_t  rs_rx_buf[256];
static volatile uint16_t rs_rx_head = 0, rs_rx_tail = 0;

static void on_uart_rx(void) {
    while (uart_is_readable(RS485_UART)) {
        uint8_t c = uart_getc(RS485_UART);
        uint16_t next = (rs_rx_head + 1) & 0xFF;
        if (next != rs_rx_tail) {
            rs_rx_buf[rs_rx_head] = c;
            rs_rx_head = next;
        }
    }
}

static void rs485_send(uint8_t cmd, uint8_t addr, const uint8_t *payload, uint8_t len) {
    uint8_t pkt[5 + PKT_MAX_PAYLOAD];
    pkt[0] = PKT_SYNC;
    pkt[1] = cmd;
    pkt[2] = addr;
    pkt[3] = len;
    memcpy(&pkt[4], payload, len);
    uint8_t crc = 0;
    for (int i = 1; i < 4 + len; i++) crc = crc8_update(crc, pkt[i]);
    pkt[4 + len] = crc;

    gpio_put(RS485_DE_PIN, 1);
    uart_write_blocking(RS485_UART, pkt, 5 + (size_t)len);
    uart_tx_wait_blocking(RS485_UART);  // TX complete (not FIFO-empty) before dropping DE
    gpio_put(RS485_DE_PIN, 0);
}

// Remote LED frame state (Core 1 only)
static uint8_t  remote_frame[LED_FRAME_LEN];
static bool     remote_dirty  = false;
static bool     remote_seen   = false;
static uint32_t last_remote_ms = 0;
static uint32_t stat_led_frames = 0, stat_polls = 0, stat_crc_errs = 0;

static void handle_packet(uint8_t cmd, uint8_t addr, const uint8_t *payload, uint8_t len) {
    if (cmd == 'L' && (addr == 0xFF || addr == g_panel_id) && len == LED_FRAME_LEN) {
        memcpy(remote_frame, payload, LED_FRAME_LEN);
        remote_dirty   = true;
        remote_seen    = true;
        last_remote_ms = to_ms_since_boot(get_absolute_time());
        stat_led_frames++;
    } else if (cmd == 'C' && addr == g_panel_id && len == 5) {
        // Set thresholds: [channel | 0xFF=all][press u16 LE][release u16 LE]
        uint8_t  ch    = payload[0];
        uint16_t press = (uint16_t)payload[1] | ((uint16_t)payload[2] << 8);
        uint16_t rel   = (uint16_t)payload[3] | ((uint16_t)payload[4] << 8);
        for (int i = 0; i < NUM_FSR; i++) {
            if (ch == 0xFF || ch == i) {
                g_press_thr[i] = press;
                g_rel_thr[i]   = rel;
            }
        }
        rs485_send('c', g_panel_id, payload, len);  // ack: echo back what was applied
    } else if (cmd == 'F' && addr == g_panel_id && len == 0) {
        uint8_t reply[9];
        for (int i = 0; i < NUM_FSR; i++) {
            uint16_t v = g_fsr_vals[i];
            reply[2 * i]     = (uint8_t)(v & 0xFF);
            reply[2 * i + 1] = (uint8_t)(v >> 8);
        }
        reply[8] = g_pressed_mask;
        rs485_send('f', g_panel_id, reply, sizeof(reply));
        stat_polls++;
    }
}

static void parse_rx(void) {
    static enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
    static uint8_t cmd, addr, len, idx;
    static uint8_t pay[PKT_MAX_PAYLOAD];

    while (rs_rx_tail != rs_rx_head) {
        uint8_t c = rs_rx_buf[rs_rx_tail];
        rs_rx_tail = (rs_rx_tail + 1) & 0xFF;
        switch (st) {
            case W_SYNC: if (c == PKT_SYNC) st = W_CMD; break;
            case W_CMD:  cmd = c; st = W_ADDR; break;
            case W_ADDR: addr = c; st = W_LEN; break;
            case W_LEN:
                len = c; idx = 0;
                if (len > PKT_MAX_PAYLOAD) { st = W_SYNC; break; }
                st = (len > 0) ? W_PAY : W_CRC;
                break;
            case W_PAY:
                pay[idx++] = c;
                if (idx == len) st = W_CRC;
                break;
            case W_CRC: {
                uint8_t crc = 0;
                crc = crc8_update(crc, cmd);
                crc = crc8_update(crc, addr);
                crc = crc8_update(crc, len);
                for (int i = 0; i < len; i++) crc = crc8_update(crc, pay[i]);
                if (crc == c) handle_packet(cmd, addr, pay, len);
                else          stat_crc_errs++;
                st = W_SYNC;
                break;
            }
        }
    }
}

// ── Core 1: RS-485 comms + LED loop ──────────────────────────────────────────
static uint32_t pixel_buf[NUM_LEDS];

static void write_pixels(PIO pio, uint sm) {
    for (int i = 0; i < NUM_LEDS; i++)
        pio_sm_put_blocking(pio, sm, pixel_buf[i]);
    sleep_us(60);  // WS2812B latch: >50µs low
}

static void core1_entry(void) {
    PIO  pio    = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    uint sm     = pio_claim_unused_sm(pio, true);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000.0f, false);

    // RS-485 UART with IRQ-driven RX (keeps the FIFO drained even while the
    // blocking LED write is in progress)
    uart_init(RS485_UART, RS485_BAUD);
    gpio_set_function(RS485_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RS485_RX_PIN, GPIO_FUNC_UART);
    gpio_init(RS485_DE_PIN);
    gpio_set_dir(RS485_DE_PIN, GPIO_OUT);
    gpio_put(RS485_DE_PIN, 0);  // receive mode
    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(RS485_UART, true, false);

    const uint8_t *patterns[2] = {PATTERN_SERPENTINE, PATTERN_READING};
    int pattern_idx  = 0;
    int chase_pos    = 0;
    int cycle_count  = 0;
    int frame        = 0;
    uint32_t last_anim_ms = 0;
    const int TAIL_LEN          = 5;
    const int CHASE_FRAME_HOLD  = 2;
    const int CYCLES_PER_PATTERN = 2;

    while (true) {
        parse_rx();

        uint32_t now = to_ms_since_boot(get_absolute_time());
        bool remote_live = remote_seen && (now - last_remote_ms) < LED_FALLBACK_MS;

        if (remote_live) {
            // Master owns the LEDs: display each frame as it arrives
            if (remote_dirty) {
                remote_dirty = false;
                for (int i = 0; i < NUM_LEDS; i++)
                    pixel_buf[i] = rgb_to_grb_word(remote_frame[3 * i],
                                                   remote_frame[3 * i + 1],
                                                   remote_frame[3 * i + 2]);
                write_pixels(pio, sm);
            }
            sleep_us(100);
            continue;
        }

        // No remote frames: local animation at ~60Hz
        if (now - last_anim_ms < 16) {
            sleep_us(100);
            continue;
        }
        last_anim_ms = now;

        if (g_pressed) {
            uint16_t max_val = 0;
            for (int i = 0; i < NUM_FSR; i++) {
                if (g_active_fsrs[i] && g_fsr_vals[i] > max_val)
                    max_val = g_fsr_vals[i];
            }
            float pressure  = max_val / 4095.0f;
            float hue       = 240.0f - pressure * 240.0f;
            float brightness = 0.15f + pressure * 0.65f;
            uint8_t r, g, b;
            hsv_to_rgb(hue, 1.0f, brightness, &r, &g, &b);
            uint32_t word = rgb_to_grb_word(r, g, b);
            for (int i = 0; i < NUM_LEDS; i++) pixel_buf[i] = word;
        } else {
            memset(pixel_buf, 0, sizeof(pixel_buf));
            const uint8_t *pattern = patterns[pattern_idx];
            for (int tail = 0; tail < TAIL_LEN; tail++) {
                int     idx  = (chase_pos - tail + NUM_LEDS) % NUM_LEDS;
                uint8_t led  = pattern[idx];
                uint8_t brt  = (uint8_t)((TAIL_LEN - tail) * 30 / TAIL_LEN);
                pixel_buf[led] = rgb_to_grb_word(0, 0, brt);
            }
            if (frame % CHASE_FRAME_HOLD == 0) {
                chase_pos = (chase_pos + 1) % NUM_LEDS;
                if (chase_pos == 0) {
                    if (++cycle_count >= CYCLES_PER_PATTERN) {
                        cycle_count = 0;
                        pattern_idx = (pattern_idx + 1) % 2;
                    }
                }
            }
        }

        write_pixels(pio, sm);
        frame++;
    }
}

// ── Core 0: FSR sampling + INT line ──────────────────────────────────────────
int main(void) {
    stdio_init_all();

    gpio_init(DEBUG_PRESS_PIN);
    gpio_set_dir(DEBUG_PRESS_PIN, GPIO_OUT);
    gpio_put(DEBUG_PRESS_PIN, 0);

    // Open-drain INT: output register held at 0, direction toggles instead.
    // Output mode sinks the line LOW (pressed); input mode is high-Z and the
    // master's pull-up takes it HIGH (idle). Pulls disabled so the RP2040
    // doesn't fight the master's 10k pull-up.
    gpio_init(INT_PIN);
    gpio_disable_pulls(INT_PIN);
    gpio_put(INT_PIN, 0);
    gpio_set_dir(INT_PIN, GPIO_IN);

    // Panel ID: internal pull-down, floating = panel 0, jumper to 3V3 OUT = panel 1
    gpio_init(PANEL_ID_PIN);
    gpio_set_dir(PANEL_ID_PIN, GPIO_IN);
    gpio_pull_down(PANEL_ID_PIN);
    sleep_us(10);  // let the pull settle before sampling
    g_panel_id = gpio_get(PANEL_ID_PIN) ? 1 : 0;

    adc_init();
    for (int i = 0; i < NUM_FSR; i++)
        adc_gpio_init(FSR_GPIO[i]);

    // Wait up to 3s for USB serial connection before continuing
    uint32_t t0 = time_us_32();
    while (!stdio_usb_connected() && (time_us_32() - t0) < 3000000)
        tight_loop_contents();

    printf("Panel firmware (C/Pico SDK 2.x)\n");
    printf("Panel ID: %d (GPIO%d %s)\n", g_panel_id, PANEL_ID_PIN,
           g_panel_id ? "tied to 3V3" : "floating/grounded");
    printf("ADC range: 12-bit (0-4095). Press > %d, release < %d\n",
           FSR_PRESS_THRESHOLD, FSR_RELEASE_THRESHOLD);

    // Detect active FSR channels
    printf("Detecting active FSR channels...\n");
    for (int i = 0; i < NUM_FSR; i++) {
        adc_select_input(i);
        uint32_t sum = 0;
        for (int s = 0; s < 50; s++) sum += adc_read();
        uint32_t avg = sum / 50;
        g_active_fsrs[i] = (avg < FSR_FLOAT_THRESHOLD);
        printf("  FSR %d (GPIO%d): resting avg = %4lu -> %s\n",
               i, (int)FSR_GPIO[i], avg,
               g_active_fsrs[i] ? "active" : "not connected");
    }

    multicore_launch_core1(core1_entry);

    // Sampling loop — prints achieved rate once per second
    uint32_t loop_count = 0;
    uint64_t report_at  = time_us_64() + 1000000;

    while (true) {
        bool any_pressed = false;
        uint8_t mask = 0;
        for (int i = 0; i < NUM_FSR; i++) {
            if (!g_active_fsrs[i]) continue;
            adc_select_input(i);
#if ADC_DUMMY_READ
            (void)adc_read();  // absorb mux charge injection; keep the second read
#endif
            uint16_t v = adc_read();
            g_fsr_vals[i] = v;
            bool want = g_fsr_pressed[i];
            if (g_fsr_pressed[i]) {
                if (v < g_rel_thr[i]) want = false;
            } else {
                if (v > g_press_thr[i]) want = true;
            }
            if (want != g_fsr_pressed[i]) {
                if (++g_fsr_streak[i] >= FSR_PERSISTENCE) {
                    g_fsr_pressed[i] = want;
                    g_fsr_streak[i] = 0;
                }
            } else {
                g_fsr_streak[i] = 0;
            }
            if (g_fsr_pressed[i]) { any_pressed = true; mask |= (uint8_t)(1u << i); }
        }
        g_pressed = any_pressed;
        g_pressed_mask = mask;
        gpio_put(DEBUG_PRESS_PIN, any_pressed ? 1 : 0);
        gpio_set_dir(INT_PIN, any_pressed ? GPIO_OUT : GPIO_IN);
        loop_count++;

        uint64_t now = time_us_64();
        if (now >= report_at) {
            printf("[%5lu Hz]  FSR: %4d %4d %4d %4d  %s | bus: %lu led, %lu polls, %lu crc err\n",
                   loop_count,
                   g_fsr_vals[0], g_fsr_vals[1], g_fsr_vals[2], g_fsr_vals[3],
                   g_pressed ? "PRESSED" : "idle",
                   stat_led_frames, stat_polls, stat_crc_errs);
            loop_count = 0;
            report_at  = now + 1000000;
        }
    }
}
