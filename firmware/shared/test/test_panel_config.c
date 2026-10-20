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

    // Defaults: 500/400, populated + enabled, on every channel. The 'W' payload
    // is 01 then (F4 01 90 01 03) x4, and zlib.crc32 of it is 0xFD704831.
    uint8_t cfg[KP_PANEL_CFG_LEN];
    kp_panel_cfg_default(cfg);
    const uint8_t ch_default[KP_CH_LEN] = {0xF4, 0x01, 0x90, 0x01, 0x03};
    bool layout = true;
    for (int i = 0; i < KP_NUM_FSR; i++)
        layout = layout && memcmp(cfg + i * KP_CH_LEN, ch_default, KP_CH_LEN) == 0;
    check(layout, "default channel bytes F4 01 90 01 03");
    check(kp_w_crc(cfg) == 0xFD704831u, "default 'W' CRC 0xFD704831");
    check(kp_panel_cfg_bad_channel(cfg) == -1, "defaults are valid");

    // The rule: release < press <= 4095, flag bits 2-7 zero.
    uint8_t ch[KP_CH_LEN];
    kp_put_u16(ch, 4095); kp_put_u16(ch + 2, 4094); ch[4] = 0;
    check(kp_channel_valid(ch), "press 4095, release 4094, flags 0: valid");
    kp_put_u16(ch, 1); kp_put_u16(ch + 2, 0); ch[4] = 3;
    check(kp_channel_valid(ch), "press 1, release 0: valid");
    kp_put_u16(ch, 4096); kp_put_u16(ch + 2, 400); ch[4] = 3;
    check(!kp_channel_valid(ch), "press 4096: invalid");
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

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures != 0;
}
