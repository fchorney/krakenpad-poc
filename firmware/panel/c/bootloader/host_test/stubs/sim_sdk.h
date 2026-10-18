// Just enough of the Pico SDK for ../../main.c to build and run on the host.
// Flash is a RAM array with NOR semantics; the UART is stdin/stdout.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef unsigned int uint;
#define count_of(a) (sizeof(a) / sizeof((a)[0]))

extern uint8_t sim_flash[4 * 1024 * 1024];
#define XIP_BASE ((uintptr_t)sim_flash)
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_PAGE_SIZE   256u
void flash_range_erase(uint32_t offset, size_t count);
void flash_range_program(uint32_t offset, const uint8_t *data, size_t count);

#define GPIO_IN  0
#define GPIO_OUT 1
#define GPIO_FUNC_UART 2
void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool out);
void gpio_put(uint pin, bool value);
bool gpio_get(uint pin);
void gpio_pull_up(uint pin);
void gpio_disable_pulls(uint pin);
void gpio_set_function(uint pin, int fn);

typedef uint64_t absolute_time_t;
void busy_wait_us(uint32_t us);
uint64_t time_us_64(void);
absolute_time_t get_absolute_time(void);
uint32_t to_ms_since_boot(absolute_time_t t);

typedef struct { volatile uint32_t dr; } uart_hw_t;
#define uart0 ((void *)0)
#define UART_PARITY_NONE 0
uint uart_init(void *u, uint baud);
void uart_deinit(void *u);
void uart_set_format(void *u, uint bits, uint stop, int parity);
void uart_set_fifo_enabled(void *u, bool on);
void uart_set_hw_flow(void *u, bool cts, bool rts);
void uart_set_irq_enables(void *u, bool rx, bool tx);
bool uart_is_readable(void *u);
uart_hw_t *uart_get_hw(void *u);
void uart_write_blocking(void *u, const uint8_t *src, size_t len);
void uart_tx_wait_blocking(void *u);

#define UART0_IRQ 20
void irq_set_exclusive_handler(uint num, void (*handler)(void));
void irq_set_enabled(uint num, bool on);

typedef struct { volatile uint32_t scratch[8]; volatile uint32_t reason; } sim_watchdog_t;
extern sim_watchdog_t sim_watchdog;
#define watchdog_hw (&sim_watchdog)
