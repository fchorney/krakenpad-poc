//! Panel config: what the master keeps for each panel and pushes to it.
//!
//! The format is defined once for the firmwares, in
//! `firmware/shared/panel_config.h`; the values here repeat it, and a test
//! reads that header so the two can't drift apart. Layout and rules:
//! `docs/RS485_PROTOCOL.md` → 'W', `docs/PANEL_CONFIG.md`.

/// The config format version this SDK reads and writes. The master states the
/// highest one it speaks in device info; the SDK won't write to a master that
/// speaks another.
pub const CONFIG_VERSION: u8 = 2;
pub const NUM_PANELS: usize = 9;
/// FSR channels per panel, in the panel's `'f'` order: ADC 0–3. On rev-1
/// panels that is South, West, North, East (`docs/DUAL_PANEL.md`).
pub const NUM_CHANNELS: usize = 4;
/// Highest 12-bit ADC reading.
pub const ADC_MAX: u16 = 4095;
/// Calibrated units: 0 at a sensor's tare (its tracked rest value), this at
/// full scale, linear in raw counts between. Thresholds are in these units,
/// and this is the highest legal press threshold (`docs/PANEL_CONFIG.md` →
/// "Tare and calibrated values").
pub const CAL_MAX: u16 = 1000;
/// A firm press, chosen to feel like a stock pad on its lowest sensitivity.
pub const DEFAULT_PRESS: u16 = 930;
pub const DEFAULT_RELEASE: u16 = 905;

/// A raw reading in calibrated units at this tare, as the firmware computes it
/// (`kp_raw_to_cal`): rounded toward zero, negative below the tare.
pub fn raw_to_calibrated(raw: u16, tare: u16) -> i16 {
    let span = ADC_MAX as i32 - tare as i32;
    ((raw as i32 - tare as i32) * CAL_MAX as i32 / span) as i16
}

const CH_POPULATED: u8 = 0x01;
const CH_ENABLED: u8 = 0x02;
const CH_LEN: usize = 5;
pub(crate) const PANEL_LEN: usize = NUM_CHANNELS * CH_LEN; // 20
pub(crate) const BLOCK_LEN: usize = NUM_PANELS * PANEL_LEN; // 180

/// One FSR channel.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ChannelConfig {
    /// Calibrated value (0–1000) at or above which the channel presses.
    pub press: u16,
    /// Calibrated value at or below which a pressed channel releases.
    pub release: u16,
    /// A sensor is physically fitted to this connector.
    pub populated: bool,
    /// Use it for presses. A disabled channel still reports telemetry.
    pub enabled: bool,
}

impl Default for ChannelConfig {
    fn default() -> Self {
        ChannelConfig {
            press: DEFAULT_PRESS,
            release: DEFAULT_RELEASE,
            populated: true,
            enabled: true,
        }
    }
}

impl ChannelConfig {
    /// The rule a panel and the master both enforce: `release < press <= 1000`.
    pub fn is_valid(&self) -> bool {
        self.release < self.press && self.press <= CAL_MAX
    }

    /// Whether the panel uses this channel for presses: populated and enabled.
    pub fn detects_presses(&self) -> bool {
        self.populated && self.enabled
    }

    fn write(&self, out: &mut [u8]) {
        out[0..2].copy_from_slice(&self.press.to_le_bytes());
        out[2..4].copy_from_slice(&self.release.to_le_bytes());
        out[4] = (if self.populated { CH_POPULATED } else { 0 })
            | (if self.enabled { CH_ENABLED } else { 0 });
    }

    /// None if a reserved flag bit is set, which no valid config has.
    fn read(b: &[u8]) -> Option<Self> {
        if b[4] & !(CH_POPULATED | CH_ENABLED) != 0 {
            return None;
        }
        Some(ChannelConfig {
            press: u16::from_le_bytes([b[0], b[1]]),
            release: u16::from_le_bytes([b[2], b[3]]),
            populated: b[4] & CH_POPULATED != 0,
            enabled: b[4] & CH_ENABLED != 0,
        })
    }
}

/// One panel's config, as the master sends it in `'W'`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct PanelConfig {
    pub channels: [ChannelConfig; NUM_CHANNELS],
}

impl PanelConfig {
    /// The 20 bytes that follow the version byte in `'W'`.
    pub(crate) fn to_bytes(self) -> [u8; PANEL_LEN] {
        let mut out = [0u8; PANEL_LEN];
        for (ch, chunk) in self.channels.iter().zip(out.chunks_mut(CH_LEN)) {
            ch.write(chunk);
        }
        out
    }

    pub(crate) fn from_bytes(b: &[u8]) -> Option<Self> {
        let mut channels = [ChannelConfig::default(); NUM_CHANNELS];
        for (ch, chunk) in channels.iter_mut().zip(b.chunks(CH_LEN)) {
            *ch = ChannelConfig::read(chunk)?;
        }
        Some(PanelConfig { channels })
    }

    /// The CRC-32 of this config as a version-1 `'W'` payload: what the panel
    /// reports once it runs it, and what config result events carry.
    pub fn crc(&self) -> u32 {
        crc32(crc32(0, &[CONFIG_VERSION]), &self.to_bytes())
    }
}

/// Every panel's config: what the master saves and what Get/Set config carry.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct PadConfig {
    /// Indexed by panel ID: 0 UL, 1 U, 2 UR, 3 L, 4 C, 5 R, 6 DL, 7 D, 8 DR.
    pub panels: [PanelConfig; NUM_PANELS],
}

impl PadConfig {
    /// The first channel breaking the rule, as (panel, channel), or None.
    pub fn first_invalid(&self) -> Option<(usize, usize)> {
        self.panels.iter().enumerate().find_map(|(p, panel)| {
            panel
                .channels
                .iter()
                .position(|c| !c.is_valid())
                .map(|c| (p, c))
        })
    }

    pub(crate) fn to_bytes(self) -> [u8; BLOCK_LEN] {
        let mut out = [0u8; BLOCK_LEN];
        for (panel, chunk) in self.panels.iter().zip(out.chunks_mut(PANEL_LEN)) {
            chunk.copy_from_slice(&panel.to_bytes());
        }
        out
    }

    pub(crate) fn from_bytes(b: &[u8]) -> Option<Self> {
        if b.len() < BLOCK_LEN {
            return None;
        }
        let mut panels = [PanelConfig::default(); NUM_PANELS];
        for (panel, chunk) in panels.iter_mut().zip(b.chunks(PANEL_LEN)) {
            *panel = PanelConfig::from_bytes(chunk)?;
        }
        Some(PadConfig { panels })
    }
}

/// Standard CRC-32 (zlib), a nibble at a time, as the firmwares compute it.
/// `crc32(crc32(0, a), b)` is the CRC of `a` followed by `b`.
pub(crate) fn crc32(prev: u32, data: &[u8]) -> u32 {
    const T: [u32; 16] = [
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158,
        0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4,
        0xA00AE278, 0xBDBDF21C,
    ];
    let mut c = !prev;
    for &b in data {
        c ^= b as u32;
        c = (c >> 4) ^ T[(c & 0xF) as usize];
        c = (c >> 4) ^ T[(c & 0xF) as usize];
    }
    !c
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc32_matches_zlib() {
        assert_eq!(crc32(0, b"123456789"), 0xCBF43926);
        assert_eq!(crc32(crc32(0, b"1234"), b"56789"), 0xCBF43926);
    }

    /// The same check value firmware/shared/test asserts: defaults on every
    /// channel, as a 'W' payload, have CRC 0x61F3A50A.
    #[test]
    fn default_panel_bytes_and_crc() {
        let p = PanelConfig::default();
        assert_eq!(
            p.to_bytes(),
            [0xA2, 0x03, 0x89, 0x03, 0x03].repeat(4).as_slice()
        );
        assert_eq!(p.crc(), 0x61F3A50A);
    }

    /// The same values firmware/shared/test asserts for kp_raw_to_cal.
    #[test]
    fn calibrated_values() {
        assert_eq!(raw_to_calibrated(100, 100), 0);
        assert_eq!(raw_to_calibrated(4095, 100), 1000);
        assert_eq!(raw_to_calibrated(3816, 100), 930);
        assert_eq!(raw_to_calibrated(3815, 100), 929);
        assert_eq!(raw_to_calibrated(90, 100), -2);
    }

    #[test]
    fn round_trip_and_reserved_bits() {
        let mut pad = PadConfig::default();
        pad.panels[3].channels[2] = ChannelConfig {
            press: 650,
            release: 410,
            populated: true,
            enabled: false,
        };
        pad.panels[8].channels[0].populated = false;
        let bytes = pad.to_bytes();
        assert_eq!(PadConfig::from_bytes(&bytes), Some(pad));
        let mut bad = bytes;
        bad[3 * PANEL_LEN + 2 * CH_LEN + 4] |= 0x04;
        assert_eq!(PadConfig::from_bytes(&bad), None);
    }

    #[test]
    fn validity_rule() {
        let ch = |press, release| ChannelConfig {
            press,
            release,
            ..Default::default()
        };
        assert!(ch(1000, 999).is_valid());
        assert!(ch(1, 0).is_valid());
        assert!(!ch(1001, 400).is_valid());
        assert!(!ch(500, 500).is_valid());
        assert!(!ch(500, 600).is_valid());
        let mut pad = PadConfig::default();
        assert_eq!(pad.first_invalid(), None);
        pad.panels[5].channels[1] = ch(300, 300);
        assert_eq!(pad.first_invalid(), Some((5, 1)));
    }

    /// The values above must be the firmwares' (firmware/shared/panel_config.h).
    #[test]
    fn matches_the_firmware_header() {
        let path = concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../../firmware/shared/panel_config.h"
        );
        let header = std::fs::read_to_string(path).expect("firmware/shared/panel_config.h");
        let define = |name: &str| -> u32 {
            let line = header
                .lines()
                .find(|l| l.split_whitespace().take(2).eq(["#define", name]))
                .unwrap_or_else(|| panic!("{name} not in panel_config.h"));
            let v = line.split_whitespace().nth(2).unwrap();
            match v.strip_prefix("0x") {
                Some(hex) => u32::from_str_radix(hex, 16).unwrap(),
                None => v.parse().unwrap(),
            }
        };
        assert_eq!(define("KP_CONFIG_VERSION"), CONFIG_VERSION as u32);
        assert_eq!(define("KP_NUM_FSR"), NUM_CHANNELS as u32);
        assert_eq!(define("KP_ADC_MAX"), ADC_MAX as u32);
        assert_eq!(define("KP_CAL_MAX"), CAL_MAX as u32);
        assert_eq!(define("KP_DEFAULT_PRESS"), DEFAULT_PRESS as u32);
        assert_eq!(define("KP_DEFAULT_RELEASE"), DEFAULT_RELEASE as u32);
        assert_eq!(define("KP_CH_POPULATED"), CH_POPULATED as u32);
        assert_eq!(define("KP_CH_ENABLED"), CH_ENABLED as u32);
        assert_eq!(define("KP_CH_LEN"), CH_LEN as u32);
    }
}
