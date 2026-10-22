// Host check of ../panel_config.h: the CRC against zlib's, the defaults' CRC
// that both firmwares and the SDK must agree on, and the 'W' validity rule.
// Built as C and as C++, because the panel compiles the header as one and the
// master as the other.
//
//   firmware/shared/test/run.sh
#include <stdio.h>
#include <string.h>
#include "panel_config.h"

static int failures = 0;

static void check(bool ok, const char *what) {
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main(void) {
    // zlib.crc32(b"123456789"), the standard check value
    const uint8_t check_str[] = "123456789";
    check(kp_crc32_from(0, check_str, 9) == 0xCBF43926u, "CRC-32 check value 0xCBF43926");

    // Split across calls, as kp_w_crc does
    check(kp_crc32_from(kp_crc32_from(0, check_str, 4), check_str + 4, 5) == 0xCBF43926u,
          "CRC-32 continued across two calls");

    // Defaults: 930/905 (calibrated), populated + enabled, on every channel.
    // The 'W' payload is 02 then (A2 03 89 03 03) x4, and zlib.crc32 of it is
    // 0x61F3A50A.
    uint8_t cfg[KP_PANEL_CFG_LEN];
    kp_panel_cfg_default(cfg);
    const uint8_t ch_default[KP_CH_LEN] = {0xA2, 0x03, 0x89, 0x03, 0x03};
    bool layout = true;
    for (int i = 0; i < KP_NUM_FSR; i++)
        layout = layout && memcmp(cfg + i * KP_CH_LEN, ch_default, KP_CH_LEN) == 0;
    check(layout, "default channel bytes A2 03 89 03 03");
    check(kp_w_crc(cfg) == 0x61F3A50Au, "default 'W' CRC 0x61F3A50A");
    check(kp_panel_cfg_bad_channel(cfg) == -1, "defaults are valid");

    // The rule: release < press <= 1000, flag bits 2-7 zero.
    uint8_t ch[KP_CH_LEN];
    kp_put_u16(ch, 1000); kp_put_u16(ch + 2, 999); ch[4] = 0;
    check(kp_channel_valid(ch), "press 1000, release 999, flags 0: valid");
    kp_put_u16(ch, 1); kp_put_u16(ch + 2, 0); ch[4] = 3;
    check(kp_channel_valid(ch), "press 1, release 0: valid");
    kp_put_u16(ch, 1001); kp_put_u16(ch + 2, 400); ch[4] = 3;
    check(!kp_channel_valid(ch), "press 1001: invalid");
    kp_put_u16(ch, 500); kp_put_u16(ch + 2, 500); ch[4] = 3;
    check(!kp_channel_valid(ch), "release == press: invalid");
    kp_put_u16(ch, 500); kp_put_u16(ch + 2, 600); ch[4] = 3;
    check(!kp_channel_valid(ch), "release > press: invalid");
    kp_put_u16(ch, 500); kp_put_u16(ch + 2, 400); ch[4] = 0x04;
    check(!kp_channel_valid(ch), "reserved flag bit 2: invalid");
    kp_put_u16(ch, 500); kp_put_u16(ch + 2, 400); ch[4] = 0x80;
    check(!kp_channel_valid(ch), "reserved flag bit 7: invalid");

    kp_panel_cfg_default(cfg);
    cfg[2 * KP_CH_LEN + 4] = 0x10;
    check(kp_panel_cfg_bad_channel(cfg) == 2, "first bad channel reported (2)");

    // Calibrated <-> raw. 0 is the tare, 1000 is full scale.
    check(kp_cal_to_raw(0, 100) == 100, "cal 0 at tare 100 -> raw 100");
    check(kp_cal_to_raw(1000, 100) == 4095, "cal 1000 at tare 100 -> raw 4095");
    check(kp_cal_to_raw(930, 100) == 3816, "cal 930 at tare 100 -> raw 3816");
    check(kp_cal_to_raw(905, 100) == 3716, "cal 905 at tare 100 -> raw 3716");
    // A high threshold barely moves with the tare: 50 counts of tare -> 5 raw.
    check(kp_cal_to_raw(900, 100) - kp_cal_to_raw(900, 50) == 5,
          "cal 900: tares 100 and 50 give raw 5 apart");
    check(kp_raw_to_cal(100, 100) == 0, "raw at the tare -> cal 0");
    check(kp_raw_to_cal(4095, 100) == 1000, "raw 4095 -> cal 1000");
    check(kp_raw_to_cal(90, 100) == -2, "below the tare -> negative");
    // The panel's raw comparison and a host's calibrated one agree exactly.
    bool agree = true;
    for (uint16_t tare = 0; tare <= 1000 && agree; tare += 7)
        for (uint16_t cal = 0; cal <= KP_CAL_MAX && agree; cal += 13) {
            uint16_t thr = kp_cal_to_raw(cal, tare);
            agree = kp_raw_to_cal(thr, tare) >= (int16_t)cal &&
                    (thr == tare || kp_raw_to_cal((uint16_t)(thr - 1), tare) < (int16_t)cal);
        }
    check(agree, "raw >= cal_to_raw(c) exactly when raw_to_cal(raw) >= c");

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures != 0;
}
