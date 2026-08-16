// LED current-draw bench test — holds one flat colour across the strip so a
// DMM in series with the 12V feed can settle. Not panel firmware: no FSRs, no
// RS-485. See docs/POWER_PROTECTION.md for why this measurement exists.
//
// Strip must be PHYSICALLY CUT to NUM_LEDS. Addressing only the first 25 of a
// 144-LED strip leaves the other 119 drawing ~2mA each of quiescent current,
// which contaminates the reading by ~0.25A.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#define LED_PIN     4       // matches panel firmware's LED_PIN
#define NUM_LEDS    25      // one panel's worth

#define REFRESH_MS  20      // resend at ~50Hz so a glitched frame self-heals

static uint32_t pixel_buf[NUM_LEDS];

// WS2812B/WS2815 expect GRB order; pack into the upper 24 bits for the TX FIFO.
static inline uint32_t rgb_to_grb_word(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
}

static void write_pixels(PIO pio, uint sm) {
    for (int i = 0; i < NUM_LEDS; i++)
        pio_sm_put_blocking(pio, sm, pixel_buf[i]);
    sleep_us(60);  // latch: >50µs low
}

static void fill(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t word = rgb_to_grb_word(r, g, b);
    for (int i = 0; i < NUM_LEDS; i++) pixel_buf[i] = word;
}

static void banner(void) {
    printf("\n=== WS2815 current test — %d LEDs on GP%d ===\n", NUM_LEDS, LED_PIN);
    printf("  o  all off          expect ~50mA   (quiescent, 2.1mA/LED)\n");
    printf("  r  255,0,0          expect ~0.40A\n");
    printf("  g  0,255,0\n");
    printf("  b  0,0,255\n");
    printf("  w  255,255,255      THE ANSWER: 0.90A => 36mA/LED, 1.18A => 47mA/LED\n");
    printf("  h  half white (128,128,128)\n");
    printf("  1  single LED white — sanity check that only one lights\n");
    printf("  t  AUTO-CYCLE red/green/blue once a second, no input needed\n");
    printf("  ?  reprint this\n");
    printf("Let white settle 60s and re-read; it should not drift.\n\n");
}

int main(void) {
    stdio_init_all();

    PIO  pio    = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    uint sm     = pio_claim_unused_sm(pio, true);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000.0f, false);

    fill(0, 0, 0);
    write_pixels(pio, sm);

    // Give the host time to attach to the CDC port before the banner scrolls by.
    sleep_ms(2000);
    banner();

    const char   *state = "off";
    bool          cycle = false;
    int           step  = 0;
    absolute_time_t next_step = get_absolute_time();

    while (true) {
        int c = getchar_timeout_us(0);
        switch (c) {
            case 'o': fill(0, 0, 0);       state = "off";          break;
            case 'r': fill(255, 0, 0);     state = "255,0,0";      break;
            case 'g': fill(0, 255, 0);     state = "0,255,0";      break;
            case 'b': fill(0, 0, 255);     state = "0,0,255";      break;
            case 'w': fill(255, 255, 255); state = "255,255,255";  break;
            case 'h': fill(128, 128, 128); state = "128,128,128";  break;
            case '1':
                fill(0, 0, 0);
                pixel_buf[0] = rgb_to_grb_word(255, 255, 255);
                state = "single LED white";
                break;
            case 't': cycle = !cycle;      state = cycle ? "AUTO-CYCLE on" : "AUTO-CYCLE off"; break;
            case '?': banner();                                    break;
            default:  c = PICO_ERROR_TIMEOUT;                      break;
        }
        if (c != PICO_ERROR_TIMEOUT && c != '?') printf("-> %s\n", state);
        if (c != PICO_ERROR_TIMEOUT && c != 't' && c != '?') cycle = false;

        // Auto-cycle: rewrite the buffer once a second with no input at all.
        // If the strip follows this, the data path and latch are fine and the
        // fault is upstream. If it freezes on the first colour, the strip is
        // not seeing a reset between frames.
        if (cycle && absolute_time_diff_us(next_step, get_absolute_time()) >= 0) {
            switch (step % 3) {
                case 0: fill(64, 0, 0); break;   // low levels: this is a data
                case 1: fill(0, 64, 0); break;   // test, not a current test
                case 2: fill(0, 0, 64); break;
            }
            printf("cycle step %d\n", step % 3);
            step++;
            next_step = make_timeout_time_ms(1000);
        }

        write_pixels(pio, sm);
        sleep_ms(REFRESH_MS);
    }
}
