// Panel config format, shared by the panel and master firmwares.
//
// One definition of the 'W' layout, its validity rule, the defaults and the
// CRC both ends compute, so the panel's boot config and the master's reset
// config can't drift apart (docs/PANEL_CONFIG.md → "Factory reset"). Plain C
// that the panel (Pico SDK, C) and the master (Arduino, C++) both compile: the
// panel's CMakeLists.txt and firmware/master/build.sh add this directory to
// the include path. test/ checks it on the host.
//
// Bus format: docs/RS485_PROTOCOL.md → 'W' and 'w'. The master's EEPROM record
// and the USB config block hold the same 20 bytes per panel
// (docs/PANEL_CONFIG.md → "The master's EEPROM record").
#ifndef KRAKENPAD_PANEL_CONFIG_H
#define KRAKENPAD_PANEL_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

// Bumped on any change to the format, everywhere it travels: 'W', the master's
// EEPROM record and the USB config report (docs/PANEL_CONFIG.md → "Config
// versioning").
#define KP_CONFIG_VERSION   1

#define KP_NUM_FSR          4
#define KP_ADC_MAX          4095

// What a panel runs from boot until the master's first 'W', and what the
// master's "reset config" writes.
#define KP_DEFAULT_PRESS    500
#define KP_DEFAULT_RELEASE  400
#define KP_CH_POPULATED     0x01
#define KP_CH_ENABLED       0x02
#define KP_DEFAULT_FLAGS    (KP_CH_POPULATED | KP_CH_ENABLED)

// 'W' payload: the version, then per channel (the 'f' order) press u16 LE,
// release u16 LE, flags u8. The 20 bytes after the version are one panel's
// entry in the master's EEPROM record and in the USB config block.
#define KP_CH_LEN           5
#define KP_PANEL_CFG_LEN    (KP_NUM_FSR * KP_CH_LEN)    // 20
#define KP_W_LEN            (1 + KP_PANEL_CFG_LEN)      // 21

// 'w' reply: status, the highest version this panel speaks, CRC-32 of the
// config now in use in 'W' form.
#define KP_WACK_LEN               6
#define KP_WACK_APPLIED           0
#define KP_WACK_UNKNOWN_VERSION   1
#define KP_WACK_INVALID           2

// 'f' reply, last byte: the pressed mask in bits 0-3; bit 7 is set from boot
// until the first accepted 'W'.
#define KP_F_PRESSED_MASK    0x0F
#define KP_F_NOT_CONFIGURED  0x80

static inline uint16_t kp_get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline void kp_put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

// One channel's 5 bytes: release < press <= 4095, reserved flag bits 0.
static inline bool kp_channel_valid(const uint8_t *ch) {
    uint16_t press = kp_get_u16(ch), rel = kp_get_u16(ch + 2);
    return rel < press && press <= KP_ADC_MAX &&
           (ch[4] & (uint8_t)~(KP_CH_POPULATED | KP_CH_ENABLED)) == 0;
}

// One panel's 20 bytes: the first channel that fails, or -1 if all pass.
static inline int kp_panel_cfg_bad_channel(const uint8_t *cfg) {
    for (int i = 0; i < KP_NUM_FSR; i++)
        if (!kp_channel_valid(cfg + i * KP_CH_LEN)) return i;
    return -1;
}

static inline void kp_panel_cfg_default(uint8_t *cfg) {
    for (int i = 0; i < KP_NUM_FSR; i++) {
        uint8_t *ch = cfg + i * KP_CH_LEN;
        kp_put_u16(ch, KP_DEFAULT_PRESS);
        kp_put_u16(ch + 2, KP_DEFAULT_RELEASE);
        ch[4] = KP_DEFAULT_FLAGS;
    }
}

// Standard CRC-32 (zlib / Python's zlib.crc32), a nibble at a time, as the
// bootloader's fw_crc32. kp_crc32_from(kp_crc32_from(0, a), b) is the CRC of
// a followed by b.
static inline uint32_t kp_crc32_from(uint32_t prev, const uint8_t *p, uint32_t n) {
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t c = ~prev;
    while (n--) {
        c ^= *p++;
        c = (c >> 4) ^ t[c & 0xFu];
        c = (c >> 4) ^ t[c & 0xFu];
    }
    return ~c;
}

// CRC-32 of the 'W' payload that carries this panel config at this version:
// what a panel reports in 'w', and what the master expects it to report.
static inline uint32_t kp_w_crc(const uint8_t *panel_cfg) {
    const uint8_t v = KP_CONFIG_VERSION;
    return kp_crc32_from(kp_crc32_from(0, &v, 1), panel_cfg, KP_PANEL_CFG_LEN);
}

#endif
