// One simulated panel: the real bootloader (../main.c) built for the host.
//
//   panel_sim <dip-id> <flash-file> [hold]
//
// Bus bytes arrive on stdin and replies leave on stdout, raw. A reader thread
// stands in for the UART RX interrupt and fills the same ring. Flash is loaded
// from and saved to <flash-file>; programming only clears bits, as on NOR.
// When the bootloader hands off to the application the flash is saved and the
// process exits 0. "hold" starts it as if an application had asked to stay
// resident (watchdog scratch[0]).
#define BOOTLOADER_HOST_TEST 1
#define main bootloader_main
#include "../main.c"
#undef main

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

uint8_t sim_flash[4 * 1024 * 1024];
sim_watchdog_t sim_watchdog;
static int sim_dip;
static const char *sim_flash_file;
static uart_hw_t sim_uart;

void flash_range_erase(uint32_t offset, size_t count) {
    if (offset % FLASH_SECTOR_SIZE || count % FLASH_SECTOR_SIZE ||
        offset + count > sizeof(sim_flash)) {
        fprintf(stderr, "panel %d: BAD ERASE %#x+%#zx\n", sim_dip, offset, count);
        exit(3);
    }
    memset(&sim_flash[offset], 0xFF, count);
}

void flash_range_program(uint32_t offset, const uint8_t *data, size_t count) {
    if (offset % FLASH_PAGE_SIZE || count % FLASH_PAGE_SIZE ||
        offset + count > sizeof(sim_flash)) {
        fprintf(stderr, "panel %d: BAD PROGRAM %#x+%#zx\n", sim_dip, offset, count);
        exit(3);
    }
    for (size_t i = 0; i < count; i++) sim_flash[offset + i] &= data[i];
}

void gpio_init(uint pin) { (void)pin; }
void gpio_set_dir(uint pin, bool out) { (void)pin; (void)out; }
void gpio_put(uint pin, bool value) { (void)pin; (void)value; }
void gpio_pull_up(uint pin) { (void)pin; }
void gpio_disable_pulls(uint pin) { (void)pin; }
void gpio_set_function(uint pin, int fn) { (void)pin; (void)fn; }
bool gpio_get(uint pin) {
    if (pin >= 18 && pin <= 21) return (sim_dip >> (21 - pin)) & 1;   // GPIO18 = bit 3
    return false;
}

uint64_t time_us_64(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
uint32_t time_us_32(void) { return (uint32_t)time_us_64(); }
void busy_wait_us(uint32_t us) { (void)us; }
absolute_time_t get_absolute_time(void) { return time_us_64(); }
uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000u); }

uint uart_init(void *u, uint baud) { (void)u; return baud; }
void uart_deinit(void *u) { (void)u; }
void uart_set_format(void *u, uint b, uint s, int p) { (void)u; (void)b; (void)s; (void)p; }
void uart_set_fifo_enabled(void *u, bool on) { (void)u; (void)on; }
void uart_set_hw_flow(void *u, bool c, bool r) { (void)u; (void)c; (void)r; }
void uart_set_irq_enables(void *u, bool rx, bool tx) { (void)u; (void)rx; (void)tx; }
bool uart_is_readable(void *u) { (void)u; return false; }
uart_hw_t *uart_get_hw(void *u) { (void)u; return &sim_uart; }
void uart_tx_wait_blocking(void *u) { (void)u; }
void uart_write_blocking(void *u, const uint8_t *src, size_t len) {
    (void)u;
    while (len) {
        ssize_t n = write(1, src, len);
        if (n <= 0) exit(4);
        src += n; len -= (size_t)n;
    }
}
void irq_set_exclusive_handler(uint num, void (*h)(void)) { (void)num; (void)h; }
void irq_set_enabled(uint num, bool on) { (void)num; (void)on; }

static void save_flash(void) {
    FILE *f = fopen(sim_flash_file, "wb");
    if (!f || fwrite(sim_flash, 1, sizeof(sim_flash), f) != sizeof(sim_flash)) exit(5);
    fclose(f);
}

void host_run_app(void) {
    save_flash();
    fprintf(stderr, "panel %d: RUN_APP\n", sim_dip);
    exit(0);
}

// The RX interrupt, as a thread: same ring, same drop-the-byte-when-full rule.
// Bytes go in without the inter-byte gap flag: pipe timing says nothing about
// the bus, so the timeout is not exercised here.
static void *reader(void *arg) {
    (void)arg;
    uint8_t buf[512];
    for (;;) {
        ssize_t n = read(0, buf, sizeof(buf));
        if (n <= 0) { save_flash(); exit(0); }     // bus gone: test over
        for (ssize_t i = 0; i < n; i++) {
            uint32_t next = (ring_head + 1u) % RING_SIZE;
            if (next == ring_tail) continue;
            ring[ring_head] = buf[i];
            ring_head = next;
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: panel_sim <dip> <flash-file> [hold]\n"); return 2; }
    sim_dip = atoi(argv[1]);
    sim_flash_file = argv[2];
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    FILE *f = fopen(sim_flash_file, "rb");
    if (f) { fread(sim_flash, 1, sizeof(sim_flash), f); fclose(f); }
    if (argc > 3 && strcmp(argv[3], "hold") == 0) sim_watchdog.scratch[0] = FW_HOLD_MAGIC;

    pthread_t t;
    pthread_create(&t, NULL, reader, NULL);
    return bootloader_main();
}
