//! One pad (one master): its reader thread, its commands and its state.
//!
//! The reader thread owns one HID handle and never runs caller code. Callers
//! write through a second handle and wait for their response on a channel.
//! One command is in flight at a time per pad, as the master expects; the
//! response is matched by transaction ID, so a late one is recognised and
//! dropped.

use std::sync::atomic::{AtomicBool, AtomicU8, AtomicU64, Ordering};
use std::sync::mpsc::{self, RecvTimeoutError, SyncSender};
use std::sync::{Arc, Condvar, Mutex, OnceLock};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crate::config::{CONFIG_VERSION, NUM_PANELS, PadConfig};
use crate::error::{DeviceError, Error, Result};
use crate::events::{Event, EventQueue};
use crate::messages::{
    ClockProbe, ConfigState, DeviceInfo, FsrSample, LedFrame, LedMode, PadStatus, SelfTest,
    parse_fsr_records,
};
use crate::protocol::{
    self, Feed, IN_PACKET, KIND_CMD, KIND_EVENT, KIND_RESP, KIND_STREAM, Message, RESP_ERROR,
    Reassembler, STREAM_FSR_RAW, cmd, evt, u32_at,
};
use crate::transport::Link;

const TIMEOUT: Duration = Duration::from_secs(1);
/// Set and reset config save to the master's EEPROM first.
const SAVE_TIMEOUT: Duration = Duration::from_secs(3);
/// The master blocks ~0.7 s running it.
const SELF_TEST_TIMEOUT: Duration = Duration::from_secs(5);
/// How long the reader blocks in one read before it checks for shutdown.
const READ_POLL_MS: i32 = 50;
/// At open, how long to keep discarding what the master queued earlier.
const DRAIN_MAX: Duration = Duration::from_millis(200);

/// A panel's latest known state.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PanelSnapshot {
    /// Answering the master's polls.
    pub present: bool,
    /// None until the master has reported it.
    pub config_state: Option<ConfigState>,
    /// The latest telemetry sample; None until telemetry has been on.
    pub sample: Option<FsrSample>,
    /// When the SDK received `sample`.
    pub received: Option<Instant>,
}

/// A copy of everything the SDK knows about a pad right now. Cheap to take,
/// from any thread, as often as wanted.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PadSnapshot {
    pub player: u8,
    pub connected: bool,
    /// As of connect, or the last [`Pad::device_info`] call.
    pub info: DeviceInfo,
    pub panels: [PanelSnapshot; NUM_PANELS],
}

/// Counters for diagnosing the link.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PadStats {
    /// Packets that didn't fit the framing.
    pub malformed: u64,
    /// Responses that arrived after their command gave up.
    pub stale_responses: u64,
    pub telemetry_samples: u64,
}

/// A panel event before it knows its pad's player ID: held until the pad is
/// registered, because events can arrive during the handshake.
#[derive(Clone, Copy, Debug)]
enum PanelEvent {
    Presence {
        panel: u8,
        present: bool,
    },
    ConfigResult {
        panel: u8,
        state: ConfigState,
        crc: u32,
    },
}

impl PanelEvent {
    fn for_player(self, player: u8) -> Event {
        match self {
            PanelEvent::Presence { panel, present } => Event::PanelPresence {
                player,
                panel,
                present,
            },
            PanelEvent::ConfigResult { panel, state, crc } => Event::ConfigResult {
                player,
                panel,
                state,
                crc,
            },
        }
    }
}

#[derive(Default)]
struct PadState {
    /// Set when the context registers the pad; events flow from then on.
    player: Option<u8>,
    info: Option<DeviceInfo>,
    config: Option<PadConfig>,
    panels: [PanelSnapshot; NUM_PANELS],
    held: Vec<PanelEvent>,
}

impl PadState {
    fn apply(&mut self, e: PanelEvent) {
        match e {
            PanelEvent::Presence { panel, present } => {
                if let Some(p) = self.panels.get_mut(panel as usize) {
                    p.present = present;
                }
            }
            PanelEvent::ConfigResult { panel, state, .. } => {
                if let Some(p) = self.panels.get_mut(panel as usize) {
                    p.config_state = Some(state);
                }
            }
        }
    }
}

struct Pending {
    txn: u8,
    tx: SyncSender<Result<Vec<u8>>>,
}

#[derive(Default)]
struct Counters {
    malformed: AtomicU64,
    stale: AtomicU64,
    samples: AtomicU64,
}

pub(crate) struct PadShared {
    pub path: String,
    writer: Mutex<Box<dyn Link>>,
    /// Held for a whole command: one in flight per pad.
    cmd_lock: Mutex<()>,
    pending: Mutex<Option<Pending>>,
    next_txn: AtomicU8,
    connected: AtomicBool,
    stop: AtomicBool,
    finished: AtomicBool,
    ready: Mutex<bool>,
    ready_cv: Condvar,
    state: Mutex<PadState>,
    events: Arc<EventQueue>,
    reader: Mutex<Option<JoinHandle<()>>>,
    stats: Counters,
}

impl PadShared {
    /// Starts the reader thread and waits until it has drained what the master
    /// queued before this open.
    pub fn start(
        path: &str,
        reader: Box<dyn Link>,
        writer: Box<dyn Link>,
        events: Arc<EventQueue>,
    ) -> Result<Arc<Self>> {
        // Start transaction IDs somewhere unpredictable, so a straggler from
        // an earlier program's session can't pass for this one's response.
        let seed = (process_epoch().elapsed().as_nanos() % 255) as u8 + 1;
        let pad = Arc::new(PadShared {
            path: path.to_owned(),
            writer: Mutex::new(writer),
            cmd_lock: Mutex::new(()),
            pending: Mutex::new(None),
            next_txn: AtomicU8::new(seed),
            connected: AtomicBool::new(true),
            stop: AtomicBool::new(false),
            finished: AtomicBool::new(false),
            ready: Mutex::new(false),
            ready_cv: Condvar::new(),
            state: Mutex::new(PadState::default()),
            events,
            reader: Mutex::new(None),
            stats: Counters::default(),
        });
        let p = pad.clone();
        let handle = thread::Builder::new()
            .name("krakenpad-reader".into())
            .spawn(move || reader_loop(p, reader))
            .map_err(|e| Error::Hid(format!("could not start the reader thread: {e}")))?;
        *pad.reader.lock().unwrap() = Some(handle);
        let ready = pad.ready.lock().unwrap();
        drop(
            pad.ready_cv
                .wait_timeout_while(ready, DRAIN_MAX * 3, |r| !*r)
                .unwrap(),
        );
        Ok(pad)
    }

    pub fn finished(&self) -> bool {
        self.finished.load(Ordering::Acquire)
    }

    /// Stops the reader and waits for it.
    pub fn shutdown(&self) {
        self.stop.store(true, Ordering::Release);
        let handle = self.reader.lock().unwrap().take();
        if let Some(h) = handle {
            let _ = h.join();
        }
    }

    pub fn player(&self) -> Option<u8> {
        self.state.lock().unwrap().player
    }

    /// Device info, then the config and each panel's state, so the first
    /// snapshot is complete. Stops after device info for a master this SDK
    /// can't speak to; the context then refuses it.
    pub fn handshake(&self) -> Result<DeviceInfo> {
        let info = self.device_info()?;
        if info.protocol_version != protocol::PROTOCOL_VERSION {
            return Ok(info);
        }
        if info.config_version == CONFIG_VERSION {
            self.get_config()?;
        }
        let status = self.pad_status()?;
        let mut st = self.state.lock().unwrap();
        for (p, s) in st.panels.iter_mut().zip(status.panels.iter()) {
            p.config_state = Some(s.config_state);
        }
        Ok(info)
    }

    /// Hands the pad out: from here its events carry its player ID. Queues
    /// [`Event::PadConnected`] and whatever was held, without waking; the
    /// caller wakes once its own locks are released.
    pub fn register(&self, player: u8) {
        let mut st = self.state.lock().unwrap();
        st.player = Some(player);
        self.events.push(Event::PadConnected { player });
        if st.info.is_some_and(|i| i.config_was_reset) {
            self.events.push(Event::ConfigWasReset { player });
        }
        for e in st.held.drain(..) {
            self.events.push(e.for_player(player));
        }
    }

    fn set_ready(&self) {
        *self.ready.lock().unwrap() = true;
        self.ready_cv.notify_all();
    }

    fn alloc_txn(&self) -> u8 {
        loop {
            let t = self.next_txn.fetch_add(1, Ordering::Relaxed);
            if t != 0 {
                return t;
            }
        }
    }

    /// All packets of one message go out under one lock, so a frame sent from
    /// another thread can't land between them.
    fn write_message(&self, kind: u8, txn: u8, msg: &[u8]) -> Result<()> {
        let mut w = self.writer.lock().unwrap();
        let size = w.out_packet();
        for p in protocol::encode(kind, txn, msg, size) {
            w.write(&p)?;
        }
        Ok(())
    }

    pub(crate) fn command(&self, code: u8, args: &[u8], timeout: Duration) -> Result<Vec<u8>> {
        if !self.connected.load(Ordering::Acquire) {
            return Err(Error::Disconnected);
        }
        let _one = self.cmd_lock.lock().unwrap();
        let txn = self.alloc_txn();
        let (tx, rx) = mpsc::sync_channel(1);
        *self.pending.lock().unwrap() = Some(Pending { txn, tx });
        // The reader may have ended between the check above and now; then
        // nothing would ever answer.
        if !self.connected.load(Ordering::Acquire) {
            self.pending.lock().unwrap().take();
            return Err(Error::Disconnected);
        }
        let mut msg = Vec::with_capacity(1 + args.len());
        msg.push(code);
        msg.extend_from_slice(args);
        if let Err(e) = self.write_message(KIND_CMD, txn, &msg) {
            self.pending.lock().unwrap().take();
            return Err(e);
        }
        let r = match rx.recv_timeout(timeout) {
            Ok(r) => r?,
            Err(RecvTimeoutError::Timeout) => {
                self.pending.lock().unwrap().take();
                return Err(Error::Timeout);
            }
            Err(RecvTimeoutError::Disconnected) => return Err(Error::Disconnected),
        };
        match r.first() {
            Some(&RESP_ERROR) if r.len() >= 3 && r[1] == code => Err(Error::Device {
                command: code,
                reason: DeviceError::from_code(r[2]),
            }),
            Some(&c) if c == code => Ok(r),
            _ => Err(Error::Protocol("response to another command")),
        }
    }

    /// Transaction 0: no response and not in flight. Frames only.
    fn send_frame(&self, code: u8, args: &[u8]) -> Result<()> {
        if !self.connected.load(Ordering::Acquire) {
            return Err(Error::Disconnected);
        }
        let mut msg = Vec::with_capacity(1 + args.len());
        msg.push(code);
        msg.extend_from_slice(args);
        self.write_message(KIND_CMD, 0, &msg)
    }

    fn status_byte(r: &[u8]) -> Result<u8> {
        r.get(1)
            .copied()
            .ok_or(Error::Protocol("response without a status"))
    }

    fn device_info(&self) -> Result<DeviceInfo> {
        let info = DeviceInfo::parse(&self.command(cmd::DEVICE_INFO, &[], TIMEOUT)?)?;
        let mut st = self.state.lock().unwrap();
        st.info = Some(info);
        for (i, p) in st.panels.iter_mut().enumerate() {
            p.present = info.panels_present >> i & 1 != 0;
        }
        Ok(info)
    }

    fn get_config(&self) -> Result<PadConfig> {
        let r = self.command(cmd::GET_CONFIG, &[], TIMEOUT)?;
        let version = Self::status_byte(&r)?;
        if version != CONFIG_VERSION {
            return Err(Error::ConfigVersion {
                master: version,
                sdk: CONFIG_VERSION,
            });
        }
        let config = PadConfig::from_bytes(&r[2..]).ok_or(Error::Protocol("config block"))?;
        self.state.lock().unwrap().config = Some(config);
        Ok(config)
    }

    fn pad_status(&self) -> Result<PadStatus> {
        PadStatus::parse(&self.command(cmd::PAD_STATUS, &[], TIMEOUT)?)
    }

    // ── Reader side ─────────────────────────────────────────────────────────

    fn dispatch(&self, m: Message) {
        match m.kind {
            KIND_RESP => self.complete(m),
            KIND_EVENT => self.on_event(&m.data),
            KIND_STREAM => self.on_stream(&m.data),
            _ => {
                self.stats.malformed.fetch_add(1, Ordering::Relaxed);
            }
        }
    }

    fn complete(&self, m: Message) {
        let mut pending = self.pending.lock().unwrap();
        match pending.take() {
            Some(p) if p.txn == m.txn => {
                let _ = p.tx.send(Ok(m.data));
            }
            other => {
                *pending = other;
                self.stats.stale.fetch_add(1, Ordering::Relaxed);
            }
        }
    }

    fn on_event(&self, d: &[u8]) {
        let e = match d.first() {
            Some(&evt::PRESENCE) if d.len() >= 3 => PanelEvent::Presence {
                panel: d[1],
                present: d[2] != 0,
            },
            Some(&evt::CONFIG_RESULT) if d.len() >= 7 => PanelEvent::ConfigResult {
                panel: d[1],
                state: ConfigState::from_code(d[2]),
                crc: u32_at(d, 3),
            },
            // Reported from device info's flag at connect instead, which also
            // covers a program that opens the pad after another one did. The
            // master's event would only repeat it.
            Some(&evt::CONFIG_RESET) => return,
            _ => {
                self.stats.malformed.fetch_add(1, Ordering::Relaxed);
                return;
            }
        };
        let mut st = self.state.lock().unwrap();
        st.apply(e);
        match st.player {
            Some(player) => {
                self.events.push(e.for_player(player));
                drop(st);
                self.events.wake();
            }
            None => st.held.push(e),
        }
    }

    fn on_stream(&self, d: &[u8]) {
        if d.first() != Some(&STREAM_FSR_RAW) {
            self.stats.malformed.fetch_add(1, Ordering::Relaxed);
            return;
        }
        let now = Instant::now();
        let mut st = self.state.lock().unwrap();
        for s in parse_fsr_records(d) {
            if let Some(p) = st.panels.get_mut(s.panel as usize) {
                p.sample = Some(s);
                p.received = Some(now);
                self.stats.samples.fetch_add(1, Ordering::Relaxed);
            }
        }
    }

    fn fail_pending(&self) {
        if let Some(p) = self.pending.lock().unwrap().take() {
            let _ = p.tx.send(Err(Error::Disconnected));
        }
    }
}

fn reader_loop(pad: Arc<PadShared>, mut link: Box<dyn Link>) {
    let mut buf = [0u8; IN_PACKET + 1];
    // Whatever the master queued before this open is stale: responses to an
    // earlier program's commands, events nobody read.
    let mut alive = true;
    let t0 = Instant::now();
    while t0.elapsed() < DRAIN_MAX {
        match link.read(&mut buf, 20) {
            Ok(0) => break,
            Ok(_) => {}
            Err(_) => {
                alive = false;
                break;
            }
        }
    }
    pad.set_ready();

    let mut reassembler = Reassembler::default();
    while alive && !pad.stop.load(Ordering::Acquire) {
        match link.read(&mut buf, READ_POLL_MS) {
            Ok(0) => {}
            Ok(n) => match reassembler.feed(&buf[..n]) {
                Feed::Complete(m) => pad.dispatch(m),
                Feed::Incomplete => {}
                Feed::Malformed => {
                    pad.stats.malformed.fetch_add(1, Ordering::Relaxed);
                }
            },
            Err(_) => break,
        }
    }

    pad.connected.store(false, Ordering::Release);
    pad.fail_pending();
    if !pad.stop.load(Ordering::Acquire)
        && let Some(player) = pad.player()
    {
        pad.events.push(Event::PadDisconnected { player });
        pad.events.wake();
    }
    pad.finished.store(true, Ordering::Release);
}

fn process_epoch() -> Instant {
    static EPOCH: OnceLock<Instant> = OnceLock::new();
    *EPOCH.get_or_init(Instant::now)
}

/// One connected pad. Cheap to clone; every clone is the same pad. Calls block
/// the calling thread until the master answers (about a millisecond to tens of
/// milliseconds), so keep them off a game's render loop.
#[derive(Clone)]
pub struct Pad {
    pub(crate) inner: Arc<PadShared>,
}

impl std::fmt::Debug for Pad {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Pad")
            .field("player", &self.player())
            .field("path", &self.inner.path)
            .finish()
    }
}

impl Pad {
    /// The master's player-ID DIP: 0–3 for P1–P4.
    pub fn player(&self) -> u8 {
        self.inner
            .player()
            .expect("a Pad is only handed out once registered")
    }

    pub fn is_connected(&self) -> bool {
        self.inner.connected.load(Ordering::Acquire)
    }

    /// As of connect, or the last [`Pad::device_info`] call.
    pub fn info(&self) -> DeviceInfo {
        self.inner
            .state
            .lock()
            .unwrap()
            .info
            .expect("set during the handshake")
    }

    pub fn snapshot(&self) -> PadSnapshot {
        let st = self.inner.state.lock().unwrap();
        PadSnapshot {
            player: st.player.expect("registered"),
            connected: self.is_connected(),
            info: st.info.expect("set during the handshake"),
            panels: st.panels,
        }
    }

    /// The config as last read or written by this SDK (at connect, then by
    /// [`Pad::get_config`], [`Pad::set_config`] or [`Pad::reset_config`]).
    /// None if the master speaks another config version.
    pub fn config(&self) -> Option<PadConfig> {
        self.inner.state.lock().unwrap().config
    }

    pub fn stats(&self) -> PadStats {
        let s = &self.inner.stats;
        PadStats {
            malformed: s.malformed.load(Ordering::Relaxed),
            stale_responses: s.stale.load(Ordering::Relaxed),
            telemetry_samples: s.samples.load(Ordering::Relaxed),
        }
    }

    /// Asks the master again; also refreshes [`Pad::info`] and panel presence.
    pub fn device_info(&self) -> Result<DeviceInfo> {
        self.inner.device_info()
    }

    /// Reads the master's copy (the panels run it once they match).
    pub fn get_config(&self) -> Result<PadConfig> {
        self.inner.get_config()
    }

    /// Saves `config` on the master and pushes it to every panel. Each panel's
    /// outcome follows as [`Event::ConfigResult`].
    ///
    /// **Not during gameplay.** Saving stalls the master's interrupts, presses
    /// included, for the length of the EEPROM write. The SDK can't tell when a
    /// song is playing, so this is the caller's responsibility
    /// (`docs/PANEL_CONFIG.md`).
    pub fn set_config(&self, config: &PadConfig) -> Result<()> {
        let master = self.info().config_version;
        if master != CONFIG_VERSION {
            return Err(Error::ConfigVersion {
                master,
                sdk: CONFIG_VERSION,
            });
        }
        if let Some((panel, channel)) = config.first_invalid() {
            return Err(Error::InvalidConfig {
                panel: panel as u8,
                channel: channel as u8,
            });
        }
        let mut args = vec![CONFIG_VERSION];
        args.extend_from_slice(&config.to_bytes());
        let r = self.inner.command(cmd::SET_CONFIG, &args, SAVE_TIMEOUT)?;
        match PadShared::status_byte(&r)? {
            0 | 3 => {
                self.inner.state.lock().unwrap().config = Some(*config);
                if r[1] == 3 {
                    Err(Error::SaveFailed)
                } else {
                    Ok(())
                }
            }
            1 => Err(Error::ConfigVersion {
                master,
                sdk: CONFIG_VERSION,
            }),
            2 if r.len() >= 4 => Err(Error::InvalidConfig {
                panel: r[2],
                channel: r[3],
            }),
            _ => Err(Error::Protocol("set config status")),
        }
    }

    /// Defaults for every panel, saved and pushed. Not during gameplay, as
    /// [`Pad::set_config`].
    pub fn reset_config(&self) -> Result<()> {
        let r = self.inner.command(cmd::RESET_CONFIG, &[], SAVE_TIMEOUT)?;
        match PadShared::status_byte(&r)? {
            0 | 3 => {
                self.inner.state.lock().unwrap().config = Some(PadConfig::default());
                if r[1] == 3 {
                    Err(Error::SaveFailed)
                } else {
                    Ok(())
                }
            }
            _ => Err(Error::Protocol("reset config status")),
        }
    }

    /// Bus health and config state, per panel.
    pub fn pad_status(&self) -> Result<PadStatus> {
        self.inner.pad_status()
    }

    /// Live FSR telemetry into [`Pad::snapshot`]. Off when a pad connects.
    pub fn set_telemetry(&self, on: bool) -> Result<()> {
        let r = self.inner.command(cmd::STREAM, &[on as u8, 0], TIMEOUT)?;
        match PadShared::status_byte(&r)? {
            0 => Ok(()),
            _ => Err(Error::Protocol("stream status")),
        }
    }

    /// LED mode on the panels in `panels` (bit n = panel n); `seconds` 0 means
    /// until changed.
    pub fn set_led_mode(&self, panels: u16, mode: LedMode, seconds: u8) -> Result<()> {
        let mask = panels & 0x1FF;
        if mask == 0 {
            return Ok(());
        }
        let mut args = mask.to_le_bytes().to_vec();
        args.extend([mode as u8, seconds]);
        let r = self.inner.command(cmd::LED_MODE, &args, TIMEOUT)?;
        match PadShared::status_byte(&r)? {
            0 => Ok(()),
            s => Err(Error::Device {
                command: cmd::LED_MODE,
                reason: DeviceError::Other(s),
            }),
        }
    }

    /// The slot ↔ panel-ID self-test. Its pulses would look like presses, so
    /// the master holds pad input off while it runs (~0.7 s): never in play.
    pub fn self_test(&self) -> Result<SelfTest> {
        SelfTest::parse(&self.inner.command(cmd::SELF_TEST, &[], SELF_TEST_TIMEOUT)?)
    }

    /// Zeroes the bus counters on the master and every panel.
    pub fn zero_counters(&self) -> Result<()> {
        self.inner
            .command(cmd::ZERO_COUNTERS, &[], TIMEOUT)
            .map(|_| ())
    }

    /// Sends LED colours without waiting. The master shows the newest frame it
    /// holds and returns to its own pattern 1 s after the last one.
    pub fn send_led_frame(&self, frame: &LedFrame) -> Result<()> {
        self.inner.send_frame(cmd::LED_FRAME, &frame.data)
    }

    /// One round trip for matching clocks. Take several and trust the
    /// shortest.
    pub fn clock_probe(&self) -> Result<ClockProbe> {
        let host = process_epoch().elapsed().as_nanos() as u64;
        let t0 = Instant::now();
        let r = self
            .inner
            .command(cmd::CLOCK_PROBE, &host.to_le_bytes(), TIMEOUT)?;
        let round_trip = t0.elapsed();
        if r.len() < 17 || r[1..9] != host.to_le_bytes() {
            return Err(Error::Protocol("clock probe echo"));
        }
        Ok(ClockProbe {
            round_trip,
            master_receive_us: u32_at(&r, 9),
            master_send_us: u32_at(&r, 13),
        })
    }
}
