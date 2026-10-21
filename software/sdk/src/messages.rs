//! What the master reports, decoded. Byte layouts: `docs/USB_PROTOCOL.md` →
//! "Command table" and "As built".

use std::time::Duration;

use crate::config::NUM_PANELS;
use crate::error::{Error, Result};
use crate::protocol::{u16_at, u32_at};

fn need(m: &[u8], len: usize, what: &'static str) -> Result<()> {
    if m.len() < len {
        Err(Error::Protocol(what))
    } else {
        Ok(())
    }
}

/// The master's check of the bus termination switches, from the panels'
/// latest status replies (`docs/RS485_PROTOCOL.md` → termination).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Termination {
    /// Exactly one panel is terminated: the last one in chain order.
    Ok,
    NoneTerminated,
    Several,
    /// One panel is terminated, but not the last one in chain order.
    WrongPanel,
    /// No status from any answering panel yet.
    Unknown,
    Other(u8),
}

impl Termination {
    fn from_code(c: u8) -> Self {
        match c {
            0 => Termination::Ok,
            1 => Termination::NoneTerminated,
            2 => Termination::Several,
            3 => Termination::WrongPanel,
            0xFF => Termination::Unknown,
            other => Termination::Other(other),
        }
    }
}

/// The USB speed the master negotiated with this computer. Full Speed still
/// does everything, more slowly (`docs/USB_PROTOCOL.md` → packet size); a
/// config app shows which one a pad got.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UsbSpeed {
    /// 12 Mbit/s: 64-byte packets both ways, joystick at 1 kHz.
    Full,
    /// 480 Mbit/s: 1024-byte packets to the master, joystick at 8 kHz.
    High,
    Other(u8),
}

impl UsbSpeed {
    fn from_code(c: u8) -> Self {
        match c {
            0 => UsbSpeed::Full,
            1 => UsbSpeed::High,
            other => UsbSpeed::Other(other),
        }
    }
}

/// The master's answer to Device info.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DeviceInfo {
    pub protocol_version: u8,
    pub usb_speed: UsbSpeed,
    /// Packet sizes on the vendor interface: master → host, and host → master.
    pub in_packet: u16,
    pub out_packet: u16,
    /// Master firmware (major, minor, patch).
    pub firmware_version: (u8, u8, u8),
    /// Tells two builds of one version apart.
    pub build_tag: u32,
    /// The highest config format version the master speaks.
    pub config_version: u8,
    /// The master's DIP: 0–3 are P1–P4, 4–7 reserved.
    pub player_id: u8,
    /// The master found no valid saved config at boot and runs on defaults;
    /// clears once a config is saved. An app should suggest recalibrating.
    pub config_was_reset: bool,
    pub telemetry_on: bool,
    pub passthrough: bool,
    /// The master's console changed the config without saving it.
    pub unsaved_edits: bool,
    /// Bit n = panel n answering polls.
    pub panels_present: u16,
    pub termination: Termination,
    /// The terminated panel when exactly one is.
    pub terminated_panel: Option<u8>,
}

impl DeviceInfo {
    pub(crate) fn parse(m: &[u8]) -> Result<Self> {
        // A master on another protocol version may lay out the rest
        // differently: report only its version, and the context refuses it.
        need(m, 2, "device info too short")?;
        if m[1] != crate::protocol::PROTOCOL_VERSION {
            return Ok(DeviceInfo::other_version(m[1]));
        }
        need(m, 21, "device info too short")?;
        Ok(DeviceInfo {
            protocol_version: m[1],
            usb_speed: UsbSpeed::from_code(m[2]),
            in_packet: u16_at(m, 3),
            out_packet: u16_at(m, 5),
            firmware_version: (m[7], m[8], m[9]),
            build_tag: u32_at(m, 10),
            config_version: m[14],
            player_id: m[15],
            config_was_reset: m[16] & 0x01 != 0,
            telemetry_on: m[16] & 0x02 != 0,
            passthrough: m[16] & 0x04 != 0,
            unsaved_edits: m[16] & 0x08 != 0,
            panels_present: u16_at(m, 17),
            termination: Termination::from_code(m[19]),
            terminated_panel: (m[20] != 0xFF).then_some(m[20]),
        })
    }

    fn other_version(version: u8) -> Self {
        DeviceInfo {
            protocol_version: version,
            usb_speed: UsbSpeed::Other(0xFF),
            in_packet: 0,
            out_packet: 0,
            firmware_version: (0, 0, 0),
            build_tag: 0,
            config_version: 0,
            player_id: 0,
            config_was_reset: false,
            telemetry_on: false,
            passthrough: false,
            unsaved_edits: false,
            panels_present: 0,
            termination: Termination::Unknown,
            terminated_panel: None,
        }
    }
}

/// Where a panel's config stands, as the master tracks it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ConfigState {
    /// The panel confirmed it runs exactly the master's copy.
    Matches,
    /// Sent; no confirmation yet. A panel on firmware too old to know `'W'`
    /// stays here.
    Waiting,
    /// The panel doesn't speak this config version: its firmware needs updating.
    RejectedVersion,
    RejectedInvalid,
    /// The panel booted and hasn't been sent its config yet (on defaults).
    NotConfigured,
    Absent,
    Other(u8),
}

impl ConfigState {
    pub(crate) fn from_code(c: u8) -> Self {
        match c {
            0 => ConfigState::Matches,
            1 => ConfigState::Waiting,
            2 => ConfigState::RejectedVersion,
            3 => ConfigState::RejectedInvalid,
            4 => ConfigState::NotConfigured,
            5 => ConfigState::Absent,
            other => ConfigState::Other(other),
        }
    }
}

/// One panel in the Pad status answer.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PanelStatus {
    /// Answered a poll within the last second.
    pub present: bool,
    /// Time since its last poll reply; None if it never answered.
    pub last_reply: Option<Duration>,
    /// Polls answered over the master's last 5 s window.
    pub polls_answered_percent: u8,
    /// The panel's own CRC error count, from its latest status reply.
    pub crc_errors: u16,
    pub config_state: ConfigState,
    /// From the panel's latest status reply; None until there is one.
    pub build_tag: Option<u32>,
    /// Its termination switch; None until there is a status reply.
    pub terminated: Option<bool>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PadStatus {
    pub panels: [PanelStatus; NUM_PANELS],
}

impl PadStatus {
    pub(crate) fn parse(m: &[u8]) -> Result<Self> {
        need(m, 1 + NUM_PANELS * 12, "pad status too short")?;
        let panel = |i: usize| {
            let o = &m[1 + i * 12..1 + (i + 1) * 12];
            let age = u16_at(o, 1);
            let known = o[11] != 0xFF;
            PanelStatus {
                present: o[0] != 0,
                last_reply: (age != 0xFFFF).then(|| Duration::from_millis(age as u64)),
                polls_answered_percent: o[3],
                crc_errors: u16_at(o, 4),
                config_state: ConfigState::from_code(o[6]),
                build_tag: known.then(|| u32_at(o, 7)),
                terminated: known.then_some(o[11] != 0),
            }
        };
        Ok(PadStatus {
            panels: std::array::from_fn(panel),
        })
    }
}

/// One panel's FSR reading, as the telemetry stream carries it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FsrSample {
    pub panel: u8,
    /// The master's clock (µs, wraps every ~71.6 minutes) when the panel's
    /// reply arrived. Tells the real spacing of samples.
    pub master_us: u32,
    /// Raw 12-bit ADC per channel, in the panel's `'f'` order.
    pub raw: [u16; 4],
    /// Bit n = channel n pressed, after the panel's hysteresis.
    pub pressed: u8,
    /// False until the panel has been sent its config since it booted.
    pub configured: bool,
}

pub(crate) fn parse_fsr_records(m: &[u8]) -> impl Iterator<Item = FsrSample> + '_ {
    m[1..]
        .as_chunks::<{ crate::protocol::STREAM_RECORD }>()
        .0
        .iter()
        .map(|r| FsrSample {
            panel: r[0],
            master_us: u32_at(r, 1),
            raw: [u16_at(r, 5), u16_at(r, 7), u16_at(r, 9), u16_at(r, 11)],
            pressed: r[13] & 0x0F,
            configured: r[13] & 0x80 == 0,
        })
}

/// The master's slot ↔ panel-ID self-test (`docs/RS485_PROTOCOL.md`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SelfTest {
    /// False if an INT line was already low, which stops the test.
    pub ran: bool,
    /// Bit per INT slot found low before the test.
    pub low_slots: u16,
    /// Bit per panel ID that acknowledged on the bus.
    pub acked: u16,
    /// The panel ID whose pulse arrived on each INT slot (UL … DR).
    pub slots: [Option<u8>; NUM_PANELS],
}

impl SelfTest {
    pub(crate) fn parse(m: &[u8]) -> Result<Self> {
        need(m, 6 + 2 * NUM_PANELS, "self-test answer too short")?;
        Ok(SelfTest {
            ran: m[1] == 0,
            low_slots: u16_at(m, 2),
            acked: u16_at(m, 4),
            slots: std::array::from_fn(|s| Some(m[6 + 2 * s]).filter(|&id| id != 0xFF)),
        })
    }

    /// True when every slot answered with its own panel.
    pub fn all_in_place(&self) -> bool {
        self.ran
            && self
                .slots
                .iter()
                .enumerate()
                .all(|(s, id)| *id == Some(s as u8))
    }
}

/// Panel LED modes (`docs/PANEL_CONFIG.md` → "LED modes").
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LedMode {
    /// The master's (or the host's) LED frames.
    Normal = 0,
    /// The panel's position, from its DIP.
    Id = 1,
    /// Each edge lit by its sensor's pressure, drawn on the panel.
    SensorTest = 2,
}

/// One clock probe: for matching the host's clock to the master's.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ClockProbe {
    pub round_trip: Duration,
    /// Master µs when it read the probe; can be up to ~7 ms late behind an LED
    /// burst, so trust the probes with the shortest round trip.
    pub master_receive_us: u32,
    pub master_send_us: u32,
}

/// LEDs for every panel: 9 × 25 RGB, panel 0 first, each panel in its wire
/// (serpentine) order (`docs/ANIMATIONS.md`). Sent with
/// [`Pad::send_led_frame`](crate::Pad::send_led_frame); the master shows the
/// newest frame it holds and returns to its own pattern 1 s after the last.
#[derive(Clone, PartialEq, Eq)]
pub struct LedFrame {
    pub(crate) data: [u8; LedFrame::LEN],
}

impl LedFrame {
    pub const LEDS_PER_PANEL: usize = 25;
    pub(crate) const LEN: usize = NUM_PANELS * Self::LEDS_PER_PANEL * 3;

    pub fn new() -> Self {
        LedFrame {
            data: [0; Self::LEN],
        }
    }

    /// `led` is the index in wire order, 0–24.
    pub fn set(&mut self, panel: usize, led: usize, rgb: [u8; 3]) {
        let at = (panel * Self::LEDS_PER_PANEL + led) * 3;
        self.data[at..at + 3].copy_from_slice(&rgb);
    }

    pub fn fill(&mut self, rgb: [u8; 3]) {
        for px in self.data.as_chunks_mut::<3>().0 {
            *px = rgb;
        }
    }
}

impl Default for LedFrame {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Debug for LedFrame {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("LedFrame { .. }")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Hand-built from the master's byte layout (master.ino → CMD_DEVICE_INFO).
    #[test]
    fn device_info_layout() {
        let m = [
            0x01, 2, 1, 64, 0, 0x00, 0x04, 0, 1, 0, 0x78, 0x56, 0x34, 0x12, 1, 2, 0x09, 0xFF, 0x01,
            0, 8,
        ];
        let d = DeviceInfo::parse(&m).unwrap();
        assert_eq!(
            (d.protocol_version, d.usb_speed, d.firmware_version),
            (2, UsbSpeed::High, (0, 1, 0))
        );
        assert_eq!((d.in_packet, d.out_packet), (64, 1024));
        assert_eq!(d.build_tag, 0x12345678);
        assert_eq!((d.config_version, d.player_id), (1, 2));
        assert!(d.config_was_reset && d.unsaved_edits && !d.telemetry_on && !d.passthrough);
        assert_eq!(d.panels_present, 0x1FF);
        assert_eq!(
            (d.termination, d.terminated_panel),
            (Termination::Ok, Some(8))
        );
        assert!(DeviceInfo::parse(&m[..20]).is_err());
        // Another version: only the version is read.
        let old = DeviceInfo::parse(&[0x01, 1, 64]).unwrap();
        assert_eq!(old.protocol_version, 1);
    }

    #[test]
    fn pad_status_layout() {
        let mut m = vec![0x04];
        for i in 0..9u8 {
            let mut o = [0u8; 12];
            o[0] = (i != 4) as u8;
            o[1..3]
                .copy_from_slice(&(if i == 4 { 0xFFFFu16 } else { 70 + i as u16 }).to_le_bytes());
            o[3] = 98;
            o[4..6].copy_from_slice(&(i as u16).to_le_bytes());
            o[6] = if i == 4 { 5 } else { 0 };
            o[7..11].copy_from_slice(&0xE3707C2Eu32.to_le_bytes());
            o[11] = match i {
                4 => 0xFF,
                8 => 1,
                _ => 0,
            };
            m.extend_from_slice(&o);
        }
        let s = PadStatus::parse(&m).unwrap();
        assert_eq!(s.panels[0].last_reply, Some(Duration::from_millis(70)));
        assert_eq!(s.panels[8].terminated, Some(true));
        assert_eq!(s.panels[3].crc_errors, 3);
        let c = s.panels[4];
        assert!(
            !c.present && c.last_reply.is_none() && c.build_tag.is_none() && c.terminated.is_none()
        );
        assert_eq!(c.config_state, ConfigState::Absent);
        assert_eq!(s.panels[1].build_tag, Some(0xE3707C2E));
    }

    #[test]
    fn fsr_records() {
        // Two records: panel 3 pressed on channel 1, panel 7 not configured.
        let mut m = vec![crate::protocol::STREAM_FSR_RAW];
        m.extend([3, 0x10, 0x27, 0, 0, 95, 0, 0xA0, 0x0F, 99, 0, 97, 0, 0x02]);
        m.extend([7, 0x11, 0x27, 0, 0, 90, 0, 91, 0, 92, 0, 93, 0, 0x80]);
        let r: Vec<_> = parse_fsr_records(&m).collect();
        assert_eq!(
            r[0],
            FsrSample {
                panel: 3,
                master_us: 10000,
                raw: [95, 4000, 99, 97],
                pressed: 2,
                configured: true
            }
        );
        assert_eq!(r[1].panel, 7);
        assert!(!r[1].configured && r[1].pressed == 0);
    }

    #[test]
    fn self_test_layout() {
        let mut m = vec![0x0B, 0, 0, 0, 0xFF, 0x01];
        for s in 0..9u8 {
            m.extend(if s == 5 { [0xFF, 0] } else { [s, 1] });
        }
        let t = SelfTest::parse(&m).unwrap();
        assert!(t.ran && !t.all_in_place());
        assert_eq!(t.slots[5], None);
        assert_eq!(t.acked, 0x1FF);
    }

    #[test]
    fn led_frame_layout() {
        let mut f = LedFrame::new();
        f.set(8, 24, [1, 2, 3]);
        assert_eq!(&f.data[LedFrame::LEN - 3..], &[1, 2, 3]);
        f.set(0, 0, [9, 8, 7]);
        assert_eq!(&f.data[..3], &[9, 8, 7]);
    }
}
