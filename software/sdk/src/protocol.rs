//! Packet framing on the master's vendor HID interface.
//!
//! Must match `firmware/master/master.ino` → "USB vendor interface" and
//! `docs/USB_PROTOCOL.md`. `tools/pad_cmd.py` is a second implementation of
//! the same framing.

/// Teensyduino's VID and the PID the master borrows during development
/// (`docs/USB_PROTOCOL.md` → "Device identification").
pub(crate) const VID: u16 = 0x16C0;
pub(crate) const PID: u16 = 0x0487;
pub(crate) const PRODUCT: &str = "KrakenPad";
/// The vendor interface's top-level collection. The joystick shares the
/// VID/PID but is Generic Desktop / Joystick.
pub(crate) const USAGE_PAGE: u16 = 0xFF00;
pub(crate) const USAGE: u16 = 0x0001;

/// The protocol version this SDK speaks (device info byte 1). Version 2
/// (2026-10-21): a two-byte payload length, and OUT packets sized by the
/// negotiated USB speed. Version 3 (2026-10-22): each panel's tares in the
/// FSR stream.
pub(crate) const PROTOCOL_VERSION: u8 = 3;
/// Packets from the master: 64 bytes at both speeds.
pub(crate) const IN_PACKET: usize = 64;
/// Packets to the master are 1024 bytes at High Speed and 64 at Full Speed.
/// The transport reads the size from the report descriptor before its first
/// write; these bound what it accepts.
pub(crate) const OUT_PACKET_MIN: usize = 64;
pub(crate) const OUT_PACKET_MAX: usize = 1024;
/// Kind, transaction ID, flags, payload length u16.
const HEADER: usize = 5;
/// No message the master sends is longer than Get config (182 bytes); this
/// only stops a corrupt stream from growing a buffer without bound.
const MAX_MESSAGE: usize = 1024;

pub(crate) const KIND_CMD: u8 = 1;
pub(crate) const KIND_RESP: u8 = 2;
pub(crate) const KIND_EVENT: u8 = 3;
pub(crate) const KIND_STREAM: u8 = 4;
const FLAG_START: u8 = 0x01;
const FLAG_END: u8 = 0x02;

/// Command codes. A response's byte 0 repeats the command's code.
pub(crate) mod cmd {
    pub const DEVICE_INFO: u8 = 0x01;
    pub const GET_CONFIG: u8 = 0x02;
    pub const SET_CONFIG: u8 = 0x03;
    pub const PAD_STATUS: u8 = 0x04;
    pub const STREAM: u8 = 0x05;
    pub const RESET_CONFIG: u8 = 0x06;
    pub const LED_FRAME: u8 = 0x08;
    pub const LED_MODE: u8 = 0x0A;
    pub const SELF_TEST: u8 = 0x0B;
    pub const ZERO_COUNTERS: u8 = 0x0C;
    pub const CLOCK_PROBE: u8 = 0x10;
}

/// A command that can't be answered in its own format: `[0x7F, code, reason]`.
pub(crate) const RESP_ERROR: u8 = 0x7F;

pub(crate) mod evt {
    pub const PRESENCE: u8 = 0x81;
    pub const CONFIG_RESULT: u8 = 0x82;
    pub const CONFIG_RESET: u8 = 0x83;
}

/// FSR telemetry: records of panel u8, master µs u32, raw u16 ×4, tare u16 ×4,
/// mask u8. (0xC1, raw only in 14-byte records, until protocol 3.)
pub(crate) const STREAM_FSR: u8 = 0xC2;
pub(crate) const STREAM_RECORD: usize = 22;

/// Splits one message into packets of `packet` bytes: kind, transaction ID,
/// flags, payload length u16, then the payload, zero-padded. The report ID
/// that hidapi wants in front is the transport's business.
pub(crate) fn encode(kind: u8, txn: u8, msg: &[u8], packet: usize) -> Vec<Vec<u8>> {
    let chunks: Vec<&[u8]> = if msg.is_empty() {
        vec![&[]]
    } else {
        msg.chunks(packet - HEADER).collect()
    };
    let last = chunks.len() - 1;
    chunks
        .iter()
        .enumerate()
        .map(|(k, chunk)| {
            let mut p = vec![0u8; packet];
            p[0] = kind;
            p[1] = txn;
            p[2] = (if k == 0 { FLAG_START } else { 0 }) | (if k == last { FLAG_END } else { 0 });
            p[3..5].copy_from_slice(&(chunk.len() as u16).to_le_bytes());
            p[HEADER..HEADER + chunk.len()].copy_from_slice(chunk);
            p
        })
        .collect()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct Message {
    pub kind: u8,
    pub txn: u8,
    pub data: Vec<u8>,
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Feed {
    Complete(Message),
    Incomplete,
    /// Counted and dropped. A START always opens a new message, so the next
    /// one recovers by itself.
    Malformed,
}

/// Joins packets back into messages. The master never interleaves the
/// packets of two messages, so one open message at a time is enough.
#[derive(Default)]
pub(crate) struct Reassembler {
    open: Option<Message>,
}

impl Reassembler {
    pub fn feed(&mut self, packet: &[u8]) -> Feed {
        if packet.len() < HEADER {
            return Feed::Malformed;
        }
        let (kind, txn, flags) = (packet[0], packet[1], packet[2]);
        let n = u16_at(packet, 3) as usize;
        if HEADER + n > packet.len() {
            self.open = None;
            return Feed::Malformed;
        }
        if flags & FLAG_START != 0 {
            self.open = Some(Message {
                kind,
                txn,
                data: Vec::new(),
            });
        }
        let continues = matches!(&self.open, Some(m) if m.kind == kind && m.txn == txn);
        if !continues {
            self.open = None;
            return Feed::Malformed;
        }
        let m = self.open.as_mut().expect("checked above");
        m.data.extend_from_slice(&packet[HEADER..HEADER + n]);
        if m.data.len() > MAX_MESSAGE {
            self.open = None;
            return Feed::Malformed;
        }
        if flags & FLAG_END != 0 {
            return Feed::Complete(self.open.take().expect("checked above"));
        }
        Feed::Incomplete
    }
}

/// Bytes in the output report, from the vendor interface's HID report
/// descriptor: the report count and size in force at its Output item. Walks
/// short items only; the master's descriptor has no long ones.
pub(crate) fn out_report_size(desc: &[u8]) -> Option<usize> {
    let (mut i, mut count, mut size) = (0, 0usize, 0usize);
    while i < desc.len() {
        let prefix = desc[i];
        let n = [0, 1, 2, 4][(prefix & 3) as usize];
        let data = desc.get(i + 1..i + 1 + n)?;
        let value = data
            .iter()
            .rev()
            .fold(0usize, |v, &b| (v << 8) | b as usize);
        match prefix & 0xFC {
            0x94 => count = value,
            0x74 => size = value,
            0x90 => return Some(count * size / 8),
            _ => {}
        }
        i += 1 + n;
    }
    None
}

pub(crate) fn u16_at(b: &[u8], at: usize) -> u16 {
    u16::from_le_bytes([b[at], b[at + 1]])
}

pub(crate) fn u32_at(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]])
}

#[cfg(test)]
mod tests {
    use super::*;

    fn reassemble(packets: &[Vec<u8>]) -> Vec<Feed> {
        let mut r = Reassembler::default();
        packets.iter().map(|p| r.feed(p)).collect()
    }

    /// The master's report descriptor (core patch, usb_desc.c) for an OUT
    /// report of `out` bytes.
    fn master_descriptor(out: u16) -> Vec<u8> {
        let [lo, hi] = out.to_le_bytes();
        vec![
            0x06, 0x00, 0xFF, 0x0A, 0x01, 0x00, 0xA1, 0x01, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF,
            0x00, 0x95, 64, 0x09, 0x01, 0x81, 0x02, 0x96, lo, hi, 0x09, 0x02, 0x91, 0x02, 0xC0,
        ]
    }

    #[test]
    fn out_report_size_from_the_descriptor() {
        assert_eq!(out_report_size(&master_descriptor(1024)), Some(1024));
        assert_eq!(out_report_size(&master_descriptor(64)), Some(64));
        // The one-byte report count of a stock RawHID descriptor.
        let mut stock = master_descriptor(64);
        stock.splice(21..24, [0x95, 64]);
        assert_eq!(out_report_size(&stock), Some(64));
        assert_eq!(
            out_report_size(&master_descriptor(64)[..20]),
            None,
            "no Output item"
        );
        assert_eq!(out_report_size(&[0x96, 0x00]), None, "truncated item");
    }

    #[test]
    fn one_packet_message() {
        let p = encode(KIND_CMD, 7, &[cmd::DEVICE_INFO], 64);
        assert_eq!(p.len(), 1);
        assert_eq!(p[0].len(), 64);
        assert_eq!(
            &p[0][..6],
            &[KIND_CMD, 7, FLAG_START | FLAG_END, 1, 0, cmd::DEVICE_INFO]
        );
        assert!(p[0][6..].iter().all(|&b| b == 0), "padding is zero");
        let msg = Message {
            kind: KIND_CMD,
            txn: 7,
            data: vec![cmd::DEVICE_INFO],
        };
        assert_eq!(reassemble(&p), vec![Feed::Complete(msg)]);
    }

    fn led_frame() -> Vec<u8> {
        // 676 bytes: the code byte plus 675 LED bytes, as the master expects.
        let mut msg = vec![cmd::LED_FRAME];
        msg.extend((0..675).map(|i| i as u8));
        msg
    }

    #[test]
    fn led_frame_is_one_packet_at_high_speed() {
        let msg = led_frame();
        let p = encode(KIND_CMD, 0, &msg, OUT_PACKET_MAX);
        assert_eq!(p.len(), 1);
        assert_eq!(p[0].len(), 1024);
        assert_eq!(p[0][2], FLAG_START | FLAG_END);
        assert_eq!(u16_at(&p[0], 3), 676);
        assert_eq!(
            reassemble(&p),
            vec![Feed::Complete(Message {
                kind: KIND_CMD,
                txn: 0,
                data: msg
            })]
        );
    }

    #[test]
    fn led_frame_spans_twelve_packets_at_full_speed() {
        let msg = led_frame();
        let p = encode(KIND_CMD, 0, &msg, OUT_PACKET_MIN);
        assert_eq!(p.len(), 12);
        assert_eq!(p[0][2], FLAG_START);
        assert!(p[1..11].iter().all(|pk| pk[2] == 0));
        assert_eq!(p[11][2], FLAG_END);
        assert_eq!(u16_at(&p[11], 3) as usize, 676 - 11 * 59);
        let fed = reassemble(&p);
        assert!(fed[..11].iter().all(|f| *f == Feed::Incomplete));
        assert_eq!(
            fed[11],
            Feed::Complete(Message {
                kind: KIND_CMD,
                txn: 0,
                data: msg
            })
        );
    }

    #[test]
    fn a_start_abandons_an_open_message() {
        let long = encode(KIND_RESP, 3, &[0xAA; 100], IN_PACKET);
        let short = encode(KIND_EVENT, 0, &[evt::PRESENCE, 2, 1], IN_PACKET);
        let fed = reassemble(&[long[0].clone(), short[0].clone()]);
        assert_eq!(fed[0], Feed::Incomplete);
        assert_eq!(
            fed[1],
            Feed::Complete(Message {
                kind: KIND_EVENT,
                txn: 0,
                data: vec![evt::PRESENCE, 2, 1]
            })
        );
    }

    #[test]
    fn a_continuation_from_another_message_is_malformed() {
        let a = encode(KIND_RESP, 3, &[0xAA; 100], IN_PACKET);
        let b = encode(KIND_RESP, 4, &[0xBB; 100], IN_PACKET);
        assert_eq!(
            reassemble(&[a[0].clone(), b[1].clone()]),
            vec![Feed::Incomplete, Feed::Malformed]
        );
        assert_eq!(
            reassemble(&[b[1].clone()]),
            vec![Feed::Malformed],
            "nothing open"
        );
    }

    #[test]
    fn a_length_past_the_packet_is_malformed() {
        let mut p = encode(KIND_EVENT, 0, &[evt::PRESENCE, 0, 1], IN_PACKET).remove(0);
        p[3] = 60;
        assert_eq!(reassemble(&[p]), vec![Feed::Malformed]);
    }
}
