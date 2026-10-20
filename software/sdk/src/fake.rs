//! A pretend master for tests: the framing and byte layouts of
//! `firmware/master/master.ino` → "USB vendor interface", with no bus behind
//! it. It shares the SDK's reading of the protocol, so it can't catch a
//! misreading of the firmware; the bench does that (`docs/HOST_SOFTWARE.md`).

use std::collections::VecDeque;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use crate::config::{BLOCK_LEN, CONFIG_VERSION, PANEL_LEN, PadConfig, PanelConfig};
use crate::error::{Error, Result};
use crate::protocol::{
    self, Feed, KIND_CMD, KIND_EVENT, KIND_RESP, KIND_STREAM, PACKET_SIZE, RESP_ERROR, Reassembler,
    STREAM_FSR_RAW, cmd, evt,
};
use crate::transport::{Link, Opener};

pub(crate) struct FakeMaster {
    pub player: u8,
    /// Bit n = panel n answering.
    pub present: u16,
    pub was_reset: bool,
    pub protocol: u8,
    /// Stop answering commands.
    pub silent: bool,
    pub led_frames: usize,
    // Internal; visible only so tests can write `..Default::default()`.
    pub config: [u8; BLOCK_LEN],
    pub host_seen: bool,
    pub stream_on: bool,
    pub unplugged: bool,
    pub clock_us: u32,
    pub to_host: VecDeque<[u8; PACKET_SIZE]>,
    pub reassembler: Reassembler,
}

impl Default for FakeMaster {
    fn default() -> Self {
        FakeMaster {
            player: 0,
            present: 0x1FF,
            was_reset: false,
            protocol: 1,
            silent: false,
            led_frames: 0,
            config: PadConfig::default().to_bytes(),
            host_seen: false,
            stream_on: false,
            unplugged: false,
            clock_us: 10_000,
            to_host: VecDeque::new(),
            reassembler: Reassembler::default(),
        }
    }
}

impl FakeMaster {
    fn send(&mut self, kind: u8, txn: u8, msg: &[u8]) {
        self.to_host.extend(protocol::encode(kind, txn, msg));
    }

    /// Events only after the host's first command, as the master.
    fn event(&mut self, msg: &[u8]) {
        if self.host_seen {
            self.send(KIND_EVENT, 0, msg);
        }
    }

    fn device_info(&self, player: u8) -> Vec<u8> {
        let mut r = vec![cmd::DEVICE_INFO, self.protocol, PACKET_SIZE as u8, 0, 1, 0];
        r.extend(0xC0FF_EE00u32.to_le_bytes());
        r.extend([
            CONFIG_VERSION,
            player,
            self.was_reset as u8 | (self.stream_on as u8) << 1,
        ]);
        r.extend(self.present.to_le_bytes());
        r.extend([0, 8]);
        r
    }

    pub fn queue_stale_device_info(&mut self, txn: u8, player: u8) {
        let r = self.device_info(player);
        self.send(KIND_RESP, txn, &r);
    }

    pub fn stream(&mut self, panel: u8, raw: [u16; 4], mask: u8) {
        if !self.stream_on {
            return;
        }
        self.clock_us += 7000;
        let mut m = vec![STREAM_FSR_RAW, panel];
        m.extend(self.clock_us.to_le_bytes());
        for v in raw {
            m.extend(v.to_le_bytes());
        }
        m.push(mask);
        self.send(KIND_STREAM, 0, &m);
    }

    pub fn presence(&mut self, panel: u8, present: bool) {
        self.present = if present {
            self.present | 1 << panel
        } else {
            self.present & !(1 << panel)
        };
        self.event(&[evt::PRESENCE, panel, present as u8]);
    }

    fn config_result(&mut self, panel: usize, state: u8) {
        let crc = PanelConfig::from_bytes(&self.config[panel * PANEL_LEN..]).map_or(0, |p| p.crc());
        let mut e = vec![evt::CONFIG_RESULT, panel as u8, state];
        e.extend(crc.to_le_bytes());
        self.event(&e);
    }

    fn handle(&mut self, txn: u8, m: &[u8]) {
        let code = m[0];
        // Transaction 0 is for frames only; the master ignores anything else.
        if txn == 0 && code != cmd::LED_FRAME {
            return;
        }
        if !self.host_seen {
            self.host_seen = true;
            if self.was_reset {
                self.event(&[evt::CONFIG_RESET]);
            }
        }
        if self.silent {
            return;
        }
        let r: Vec<u8> = match code {
            cmd::DEVICE_INFO => self.device_info(self.player),
            cmd::GET_CONFIG => [&[code, CONFIG_VERSION][..], &self.config].concat(),
            cmd::SET_CONFIG => return self.set_config(txn, &m[1..]),
            cmd::PAD_STATUS => {
                let mut r = vec![code];
                for i in 0..9 {
                    let on = self.present >> i & 1 != 0;
                    r.push(on as u8);
                    r.extend((if on { 40u16 } else { 0xFFFF }).to_le_bytes());
                    r.push(if on { 100 } else { 0 });
                    r.extend(0u16.to_le_bytes());
                    r.push(if on { 0 } else { 5 });
                    r.extend((if on { 0xE370_7C2Eu32 } else { 0 }).to_le_bytes());
                    r.push(if on { (i == 8) as u8 } else { 0xFF });
                }
                r
            }
            cmd::STREAM => {
                self.stream_on = m[1] != 0;
                vec![code, (m[2] != 0) as u8]
            }
            cmd::RESET_CONFIG => {
                self.config = PadConfig::default().to_bytes();
                self.was_reset = false;
                vec![code, 0]
            }
            cmd::LED_FRAME if m.len() == 1 + 675 => {
                self.led_frames += 1;
                vec![code, 0]
            }
            cmd::LED_FRAME => vec![RESP_ERROR, code, 2],
            cmd::LED_MODE | cmd::ZERO_COUNTERS => vec![code, 0],
            cmd::SELF_TEST => {
                let mut r = vec![code, 0, 0, 0, 0xFF, 0x01];
                for s in 0..9u8 {
                    r.extend([s, 1]);
                }
                r
            }
            cmd::CLOCK_PROBE => {
                let mut r = vec![code];
                r.extend(&m[1..9]);
                r.extend(self.clock_us.to_le_bytes());
                r.extend((self.clock_us + 12).to_le_bytes());
                r
            }
            _ => vec![RESP_ERROR, code, 1],
        };
        if txn != 0 {
            self.send(KIND_RESP, txn, &r);
        }
    }

    fn set_config(&mut self, txn: u8, a: &[u8]) {
        if a.first() != Some(&CONFIG_VERSION) {
            return self.send(KIND_RESP, txn, &[cmd::SET_CONFIG, 1]);
        }
        if a.len() != 1 + BLOCK_LEN {
            return self.send(KIND_RESP, txn, &[RESP_ERROR, cmd::SET_CONFIG, 2]);
        }
        for p in 0..9 {
            let bad = PanelConfig::from_bytes(&a[1 + p * PANEL_LEN..])
                .map(|pc| pc.channels.iter().position(|c| !c.is_valid()))
                .unwrap_or(Some(0));
            if let Some(ch) = bad {
                return self.send(KIND_RESP, txn, &[cmd::SET_CONFIG, 2, p as u8, ch as u8]);
            }
        }
        self.config.copy_from_slice(&a[1..]);
        self.was_reset = false;
        // As the master: answering panels drop to waiting, then the response,
        // then each panel confirms once its push comes round.
        let present: Vec<usize> = (0..9).filter(|i| self.present >> i & 1 != 0).collect();
        for &p in &present {
            self.config_result(p, 1);
        }
        self.send(KIND_RESP, txn, &[cmd::SET_CONFIG, 0]);
        for &p in &present {
            self.config_result(p, 0);
        }
    }
}

type Plugged = Vec<(String, Arc<Mutex<FakeMaster>>)>;

/// The USB bus: masters plugged in by path.
#[derive(Clone, Default)]
pub(crate) struct FakeBus {
    masters: Arc<Mutex<Plugged>>,
}

impl FakeBus {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn plug(&self, path: &str, master: FakeMaster) {
        let mut ms = self.masters.lock().unwrap();
        ms.retain(|(p, _)| p != path);
        ms.push((path.to_owned(), Arc::new(Mutex::new(master))));
    }

    pub fn unplug(&self, path: &str) {
        let mut ms = self.masters.lock().unwrap();
        if let Some(i) = ms.iter().position(|(p, _)| p == path) {
            ms.remove(i).1.lock().unwrap().unplugged = true;
        }
    }

    pub fn with<R>(&self, path: &str, f: impl FnOnce(&mut FakeMaster) -> R) -> R {
        let m = self
            .masters
            .lock()
            .unwrap()
            .iter()
            .find(|(p, _)| p == path)
            .expect("plugged in")
            .1
            .clone();
        let mut m = m.lock().unwrap();
        f(&mut m)
    }

    pub fn opener(&self) -> FakeOpener {
        FakeOpener { bus: self.clone() }
    }
}

pub(crate) struct FakeOpener {
    bus: FakeBus,
}

impl Opener for FakeOpener {
    fn scan(&mut self) -> Vec<String> {
        self.bus
            .masters
            .lock()
            .unwrap()
            .iter()
            .map(|(p, _)| p.clone())
            .collect()
    }

    fn open(&mut self, path: &str) -> Result<(Box<dyn Link>, Box<dyn Link>)> {
        let ms = self.bus.masters.lock().unwrap();
        let m = ms
            .iter()
            .find(|(p, _)| p == path)
            .ok_or(Error::Hid("no such path".into()))?
            .1
            .clone();
        Ok((
            Box::new(FakeLink {
                master: m.clone(),
                reads: true,
            }),
            Box::new(FakeLink {
                master: m,
                reads: false,
            }),
        ))
    }
}

struct FakeLink {
    master: Arc<Mutex<FakeMaster>>,
    reads: bool,
}

impl Link for FakeLink {
    fn read(&mut self, buf: &mut [u8], timeout_ms: i32) -> Result<usize> {
        let deadline = Instant::now() + Duration::from_millis(timeout_ms.max(0) as u64);
        loop {
            {
                let mut m = self.master.lock().unwrap();
                if m.unplugged {
                    return Err(Error::Hid("unplugged".into()));
                }
                if self.reads
                    && let Some(p) = m.to_host.pop_front()
                {
                    buf[..PACKET_SIZE].copy_from_slice(&p);
                    return Ok(PACKET_SIZE);
                }
            }
            if Instant::now() >= deadline {
                return Ok(0);
            }
            thread::sleep(Duration::from_millis(1));
        }
    }

    fn write(&mut self, packet: &[u8; PACKET_SIZE]) -> Result<()> {
        let mut m = self.master.lock().unwrap();
        if m.unplugged {
            return Err(Error::Hid("unplugged".into()));
        }
        if let Feed::Complete(msg) = m.reassembler.feed(packet)
            && msg.kind == KIND_CMD
            && !msg.data.is_empty()
        {
            m.handle(msg.txn, &msg.data);
        }
        Ok(())
    }
}
