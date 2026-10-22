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
// versioning"). 2 (2026-10-22): thresholds in calibrated units, not raw counts.
#define KP_CONFIG_VERSION   2

#define KP_NUM_FSR          4
#define KP_ADC_MAX          4095

// Calibrated units (docs/PANEL_CONFIG.md → "Tare and calibrated values"):
// 0 at the sensor's tracked rest value (its tare), KP_CAL_MAX at full scale,
// linear in raw counts between them. Thresholds are in these units.
#define KP_CAL_MAX          1000

// What a panel runs from boot until the master's first 'W', and what the
// master's "reset config" writes. 930 is ~3820 raw at a tare of 100: a firm
// press, chosen to feel like a stock pad on its lowest sensitivity (the user,
// 2026-10-22). Release 905 is ~100 raw counts below it.
#define KP_DEFAULT_PRESS    930
#define KP_DEFAULT_RELEASE  905
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

// 'f' reply: raw u16 x4, tare u16 x4 (both LE, channel order), then a byte
// with the pressed mask in bits 0-3; bit 7 is set from boot until the first
// accepted 'W'. Panel firmware before config version 2 sent 9 bytes (no tare).
#define KP_F_LEN             17
#define KP_F_LEN_V1          9
#define KP_F_TARE            8    // offset of the tares
#define KP_F_MASK            16   // offset of the mask byte (8 in the 9-byte form)
#define KP_F_PRESSED_MASK    0x0F
#define KP_F_NOT_CONFIGURED  0x80

static inline uint16_t kp_get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline void kp_put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

// One channel's 5 bytes: release < press <= 1000, reserved flag bits 0.
static inline bool kp_channel_valid(const uint8_t *ch) {
    uint16_t press = kp_get_u16(ch), rel = kp_get_u16(ch + 2);
    return rel < press && press <= KP_CAL_MAX &&
           (ch[4] & (uint8_t)~(KP_CH_POPULATED | KP_CH_ENABLED)) == 0;
}

// Calibrated value -> the lowest raw count whose calibrated value reaches it.
// Rounded up, so raw >= kp_cal_to_raw(c, t) exactly when kp_raw_to_cal(raw, t)
// >= c: the panel compares raw counts and a host reading calibrated values
// agrees with it to the count. The span stretches to the same top on every
// sensor, so a high threshold lands at nearly the same raw count whatever the
// tare: at 900, tares of 50 and 100 give raw thresholds 5 counts apart, not 50
// (docs/PANEL_CONFIG.md → "Tare and calibrated values").
static inline uint16_t kp_cal_to_raw(uint16_t cal, uint16_t tare) {
    uint32_t span = (uint32_t)(KP_ADC_MAX - tare);
    return (uint16_t)(tare + ((uint32_t)cal * span + KP_CAL_MAX - 1) / KP_CAL_MAX);
}

// Raw count -> calibrated value at this tare, rounded toward zero. Negative
// below the tare (noise), so a reader sees it rather than a clamped 0.
static inline int16_t kp_raw_to_cal(uint16_t raw, uint16_t tare) {
    int32_t span = KP_ADC_MAX - (int32_t)tare;
    return (int16_t)(((int32_t)raw - (int32_t)tare) * KP_CAL_MAX / span);
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
