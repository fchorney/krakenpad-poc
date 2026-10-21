//! The context: finds pads, keeps them by player ID, and owns the event queue.

use std::collections::HashSet;
use std::sync::{Arc, Condvar, Mutex};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use crate::error::{Error, Result};
use crate::events::{Event, EventQueue, RejectReason};
use crate::pad::{Pad, PadShared};
use crate::protocol::PROTOCOL_VERSION;
use crate::transport::{HidOpener, Opener};

/// How often the context looks for pads plugged in since.
const SCAN_INTERVAL: Duration = Duration::from_secs(1);
/// Player IDs 0–3 are P1–P4; 4–7 are reserved.
const MAX_PLAYERS: u8 = 4;

struct Shared {
    /// Registered pads, at most one per player ID.
    pads: Mutex<Vec<Arc<PadShared>>>,
    events: Arc<EventQueue>,
    stop: Mutex<bool>,
    stop_cv: Condvar,
}

impl Shared {
    fn stopped(&self) -> bool {
        *self.stop.lock().unwrap()
    }
}

/// One per program: it manages every pad (up to four, by the master's
/// player-ID DIP). A background thread finds pads as they are plugged in and
/// notices when they go; [`Event`]s report both. Safe to share between
/// threads. Dropping it closes every pad.
pub struct Context {
    shared: Arc<Shared>,
    manager: Option<JoinHandle<()>>,
}

impl Context {
    pub fn new() -> Result<Self> {
        Self::with_opener(Box::new(HidOpener::new()?), SCAN_INTERVAL)
    }

    pub(crate) fn with_opener(opener: Box<dyn Opener>, interval: Duration) -> Result<Self> {
        let shared = Arc::new(Shared {
            pads: Mutex::new(Vec::new()),
            events: Arc::new(EventQueue::new()),
            stop: Mutex::new(false),
            stop_cv: Condvar::new(),
        });
        let s = shared.clone();
        let manager = thread::Builder::new()
            .name("krakenpad-manager".into())
            .spawn(move || manager_loop(s, opener, interval))
            .map_err(|e| Error::Hid(format!("could not start the manager thread: {e}")))?;
        Ok(Context {
            shared,
            manager: Some(manager),
        })
    }

    /// The connected pads, in player order.
    pub fn pads(&self) -> Vec<Pad> {
        let mut pads: Vec<Pad> = self
            .shared
            .pads
            .lock()
            .unwrap()
            .iter()
            .filter(|p| !p.finished())
            .map(|p| Pad { inner: p.clone() })
            .collect();
        pads.sort_by_key(|p| p.player());
        pads
    }

    pub fn pad(&self, player: u8) -> Option<Pad> {
        self.pads().into_iter().find(|p| p.player() == player)
    }

    /// The next event, or None at once. About one lock when there is none, so
    /// calling it every frame costs nothing.
    pub fn poll_event(&self) -> Option<Event> {
        self.shared.events.poll()
    }

    /// The next event, waiting up to `timeout` for one. For a thread of the
    /// program's own that does nothing else.
    pub fn wait_event(&self, timeout: Duration) -> Option<Event> {
        self.shared.events.wait(timeout)
    }

    /// A function the SDK calls, on its own thread, each time events arrive:
    /// for posting a wake-up into a program's own event loop, which then
    /// drains [`Context::poll_event`]. It should only signal and return, and
    /// must not call back into the SDK. None removes it.
    pub fn set_wake(&self, wake: Option<Box<dyn Fn() + Send + Sync>>) {
        self.shared.events.set_wake(wake.map(Arc::from));
    }

    /// Events dropped because the queue was full (nobody was reading it).
    pub fn events_dropped(&self) -> u64 {
        self.shared
            .events
            .dropped
            .load(std::sync::atomic::Ordering::Relaxed)
    }
}

impl Drop for Context {
    fn drop(&mut self) {
        *self.shared.stop.lock().unwrap() = true;
        self.shared.stop_cv.notify_all();
        if let Some(m) = self.manager.take() {
            let _ = m.join();
        }
        for p in self.shared.pads.lock().unwrap().drain(..) {
            p.shutdown();
        }
    }
}

fn manager_loop(shared: Arc<Shared>, mut opener: Box<dyn Opener>, interval: Duration) {
    // Refused masters, by path, until unplugged; otherwise each scan would
    // refuse (and report) the same one again.
    let mut refused: HashSet<String> = HashSet::new();
    while !shared.stopped() {
        // Forget pads whose reader has ended; a replug is a new connect.
        let gone: Vec<Arc<PadShared>> = {
            let mut pads = shared.pads.lock().unwrap();
            let (gone, keep) = pads.drain(..).partition(|p| p.finished());
            *pads = keep;
            gone
        };
        for p in gone {
            p.shutdown();
        }

        let found = opener.scan();
        refused.retain(|path| found.contains(path));
        let open: HashSet<String> = shared
            .pads
            .lock()
            .unwrap()
            .iter()
            .map(|p| p.path.clone())
            .collect();
        for path in found {
            if shared.stopped() {
                break;
            }
            if !open.contains(&path)
                && !refused.contains(&path)
                && !connect(&shared, opener.as_mut(), &path)
            {
                refused.insert(path);
            }
        }

        let stop = shared.stop.lock().unwrap();
        drop(
            shared
                .stop_cv
                .wait_timeout_while(stop, interval, |s| !*s)
                .unwrap(),
        );
    }
}

/// Opens, handshakes and registers one master. False if it was refused for
/// good (until replugged); a failure that may pass, such as a timeout while
/// the master is busy, returns true and is retried on the next scan.
fn connect(shared: &Shared, opener: &mut dyn Opener, path: &str) -> bool {
    let Ok((reader, writer)) = opener.open(path) else {
        return true;
    };
    let Ok(pad) = PadShared::start(path, reader, writer, shared.events.clone()) else {
        return true;
    };
    let info = match pad.handshake() {
        Ok(info) => info,
        Err(_) => {
            pad.shutdown();
            return true;
        }
    };

    let player = info.player_id;
    let mut pads = shared.pads.lock().unwrap();
    let reason = if info.protocol_version != PROTOCOL_VERSION {
        Some(RejectReason::Protocol {
            version: info.protocol_version,
        })
    } else if player >= MAX_PLAYERS {
        Some(RejectReason::ReservedPlayer)
    } else if pads
        .iter()
        .any(|p| p.player() == Some(player) && !p.finished())
    {
        Some(RejectReason::DuplicatePlayer)
    } else {
        None
    };
    if let Some(reason) = reason {
        drop(pads);
        pad.shutdown();
        let player = (!matches!(reason, RejectReason::Protocol { .. })).then_some(player);
        shared.events.push(Event::PadRejected { player, reason });
        shared.events.wake();
        return false;
    }
    // Listed and registered under one lock: a program reacting to
    // PadConnected finds the pad, and no listed pad lacks its player ID.
    pads.push(pad.clone());
    pad.register(player);
    drop(pads);
    shared.events.wake();
    true
}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::time::Instant;

    use super::*;
    use crate::config::{ChannelConfig, PadConfig};
    use crate::error::DeviceError;
    use crate::fake::{FakeBus, FakeMaster};
    use crate::messages::{ConfigState, LedFrame, LedMode};

    const WAIT: Duration = Duration::from_secs(3);

    fn context(bus: &FakeBus) -> Context {
        Context::with_opener(Box::new(bus.opener()), Duration::from_millis(20)).unwrap()
    }

    fn next(ctx: &Context) -> Event {
        ctx.wait_event(WAIT).expect("an event")
    }

    /// Skips events until one matches.
    fn until(ctx: &Context, f: impl Fn(&Event) -> bool) -> Event {
        let deadline = Instant::now() + WAIT;
        loop {
            let e = ctx
                .wait_event(deadline.saturating_duration_since(Instant::now()))
                .expect("the event");
            if f(&e) {
                return e;
            }
        }
    }

    #[test]
    fn connects_with_a_complete_first_snapshot() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 1,
                present: 0b1_0111_1111,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        assert_eq!(next(&ctx), Event::PadConnected { player: 1 });
        let pad = ctx.pad(1).unwrap();
        assert_eq!(ctx.pads().len(), 1);
        let s = pad.snapshot();
        assert!(s.connected && s.panels[0].present && !s.panels[7].present);
        assert_eq!(s.panels[0].config_state, Some(ConfigState::Matches));
        assert_eq!(s.panels[7].config_state, Some(ConfigState::Absent));
        assert_eq!(pad.config(), Some(PadConfig::default()));
        assert_eq!(pad.info().firmware_version, (0, 1, 0));
    }

    #[test]
    fn stale_packets_from_before_the_open_are_ignored() {
        // A device-info response claiming player 3 under every transaction
        // ID, queued before the SDK opened the pad. The real one says 1.
        let bus = FakeBus::new();
        let mut master = FakeMaster {
            player: 1,
            ..Default::default()
        };
        for txn in 1..=255 {
            master.queue_stale_device_info(txn, 3);
        }
        bus.plug("a", master);
        let ctx = context(&bus);
        assert_eq!(next(&ctx), Event::PadConnected { player: 1 });
    }

    #[test]
    fn config_was_reset_follows_connected_once() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 0,
                was_reset: true,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        assert_eq!(next(&ctx), Event::PadConnected { player: 0 });
        assert_eq!(next(&ctx), Event::ConfigWasReset { player: 0 });
        // The master also sent its own reset event; the SDK doesn't repeat it.
        assert_eq!(ctx.wait_event(Duration::from_millis(200)), None);
    }

    #[test]
    fn set_config_saves_and_reports_each_panel() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 2,
                present: 0b11,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        next(&ctx);
        let pad = ctx.pad(2).unwrap();
        let mut c = pad.get_config().unwrap();
        c.panels[1].channels[3] = ChannelConfig {
            press: 700,
            release: 450,
            populated: true,
            enabled: false,
        };
        pad.set_config(&c).unwrap();
        assert_eq!(pad.get_config().unwrap(), c);
        assert_eq!(pad.config(), Some(c));
        let e = until(&ctx, |e| {
            matches!(
                e,
                Event::ConfigResult {
                    panel: 1,
                    state: ConfigState::Matches,
                    ..
                }
            )
        });
        assert_eq!(
            e,
            Event::ConfigResult {
                player: 2,
                panel: 1,
                state: ConfigState::Matches,
                crc: c.panels[1].crc()
            }
        );
        assert_eq!(
            pad.snapshot().panels[1].config_state,
            Some(ConfigState::Matches)
        );

        c.panels[4].channels[0].release = 900;
        assert_eq!(
            pad.set_config(&c),
            Err(Error::InvalidConfig {
                panel: 4,
                channel: 0
            })
        );
        pad.reset_config().unwrap();
        assert_eq!(pad.get_config().unwrap(), PadConfig::default());
    }

    #[test]
    fn telemetry_lands_in_the_snapshot() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 0,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        next(&ctx);
        let pad = ctx.pad(0).unwrap();
        assert_eq!(pad.snapshot().panels[4].sample, None);
        pad.set_telemetry(true).unwrap();
        bus.with("a", |m| m.stream(4, [95, 4000, 99, 97], 0x02));
        let deadline = Instant::now() + WAIT;
        while pad.snapshot().panels[4].sample.is_none() {
            assert!(Instant::now() < deadline, "no sample");
            thread::sleep(Duration::from_millis(5));
        }
        let s = pad.snapshot().panels[4].sample.unwrap();
        assert_eq!(
            (s.raw, s.pressed, s.configured),
            ([95, 4000, 99, 97], 0x02, true)
        );
        assert_eq!(pad.stats().telemetry_samples, 1);
    }

    #[test]
    fn panel_presence_events() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 3,
                present: 0x1FF,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        next(&ctx);
        bus.with("a", |m| m.presence(6, false));
        assert_eq!(
            next(&ctx),
            Event::PanelPresence {
                player: 3,
                panel: 6,
                present: false
            }
        );
        assert!(!ctx.pad(3).unwrap().snapshot().panels[6].present);
    }

    #[test]
    fn refusals_timeouts_and_frames() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 0,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        next(&ctx);
        let pad = ctx.pad(0).unwrap();
        assert_eq!(
            pad.inner.command(0x55, &[], Duration::from_secs(1)),
            Err(Error::Device {
                command: 0x55,
                reason: DeviceError::UnknownCommand
            })
        );
        pad.set_led_mode(0x1FF, LedMode::Id, 3).unwrap();
        pad.zero_counters().unwrap();
        let t = pad.self_test().unwrap();
        assert!(t.all_in_place());
        let probe = pad.clock_probe().unwrap();
        assert!(probe.master_send_us >= probe.master_receive_us);
        let mut frame = LedFrame::new();
        frame.fill([0, 0, 64]);
        pad.send_led_frame(&frame).unwrap();
        pad.send_led_frame(&frame).unwrap();
        pad.zero_counters().unwrap(); // a command after the frames, so they are in
        assert_eq!(bus.with("a", |m| m.led_frames), 2);

        bus.with("a", |m| m.silent = true);
        assert_eq!(
            pad.inner.command(0x01, &[], Duration::from_millis(100)),
            Err(Error::Timeout)
        );
        bus.with("a", |m| m.silent = false);
        pad.device_info().unwrap();
        assert_eq!(
            pad.stats().stale_responses,
            0,
            "a silent master sends nothing late"
        );
    }

    #[test]
    fn packet_size_follows_the_usb_speed() {
        for (high_speed, speed, packets) in [
            (true, crate::UsbSpeed::High, vec![1024]),
            (false, crate::UsbSpeed::Full, vec![64; 12]),
        ] {
            let bus = FakeBus::new();
            bus.plug(
                "a",
                FakeMaster {
                    high_speed,
                    ..Default::default()
                },
            );
            let ctx = context(&bus);
            next(&ctx);
            let pad = ctx.pad(0).unwrap();
            assert_eq!(pad.info().usb_speed, speed);
            bus.with("a", |m| m.packets_from_host.clear());
            pad.send_led_frame(&LedFrame::new()).unwrap();
            assert_eq!(bus.with("a", |m| m.packets_from_host.clone()), packets);
        }
    }

    #[test]
    fn unplug_and_replug() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 1,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        next(&ctx);
        let pad = ctx.pad(1).unwrap();
        bus.unplug("a");
        assert_eq!(next(&ctx), Event::PadDisconnected { player: 1 });
        assert!(!pad.is_connected());
        assert_eq!(pad.get_config(), Err(Error::Disconnected));
        assert!(ctx.pad(1).is_none());
        bus.plug(
            "a",
            FakeMaster {
                player: 1,
                ..Default::default()
            },
        );
        assert_eq!(next(&ctx), Event::PadConnected { player: 1 });
        assert!(ctx.pad(1).unwrap().is_connected());
    }

    #[test]
    fn refuses_duplicate_and_reserved_players_and_other_protocols() {
        let bus = FakeBus::new();
        bus.plug(
            "a",
            FakeMaster {
                player: 0,
                ..Default::default()
            },
        );
        let ctx = context(&bus);
        assert_eq!(next(&ctx), Event::PadConnected { player: 0 });
        bus.plug(
            "b",
            FakeMaster {
                player: 0,
                ..Default::default()
            },
        );
        assert_eq!(
            next(&ctx),
            Event::PadRejected {
                player: Some(0),
                reason: RejectReason::DuplicatePlayer
            }
        );
        bus.plug(
            "c",
            FakeMaster {
                player: 7,
                ..Default::default()
            },
        );
        assert_eq!(
            next(&ctx),
            Event::PadRejected {
                player: Some(7),
                reason: RejectReason::ReservedPlayer
            }
        );
        bus.plug(
            "d",
            FakeMaster {
                player: 1,
                protocol: 1,
                ..Default::default()
            },
        );
        assert_eq!(
            next(&ctx),
            Event::PadRejected {
                player: None,
                reason: RejectReason::Protocol { version: 1 }
            }
        );
        // Each refused once, not again on every scan.
        assert_eq!(ctx.wait_event(Duration::from_millis(200)), None);
        assert_eq!(ctx.pads().len(), 1);
    }

    #[test]
    fn wake_is_called_for_events() {
        let bus = FakeBus::new();
        let ctx = context(&bus);
        let calls = Arc::new(AtomicUsize::new(0));
        let c = calls.clone();
        ctx.set_wake(Some(Box::new(move || {
            c.fetch_add(1, Ordering::SeqCst);
        })));
        bus.plug(
            "a",
            FakeMaster {
                player: 0,
                ..Default::default()
            },
        );
        assert_eq!(next(&ctx), Event::PadConnected { player: 0 });
        assert!(calls.load(Ordering::SeqCst) >= 1);
    }
}
