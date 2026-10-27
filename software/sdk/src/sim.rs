//! Simulated pads, for building and trying apps without a pad (feature `sim`).
//!
//! Each one is the SDK's test fake master behind the real SDK: the same
//! handshake, commands, events and telemetry stream, with no USB and no bus.
//! Its panels behave roughly like rev-1 ones: a fitted sensor rests near its
//! tare with a little noise, an empty socket reads near the floor, and presses
//! follow the master's config with hysteresis. Like the fake it builds on, it
//! shares the SDK's reading of the protocol, so it can't stand in for the
//! bench.
//!
//! ```no_run
//! use krakenpad::sim::{SimPad, Simulator};
//!
//! let sim = Simulator::new();
//! sim.plug(SimPad { player: 0, ..Default::default() });
//! let ctx = sim.context()?;
//! sim.set_load(0, 4, [1000; 4]); // full weight on C
//! # Ok::<(), krakenpad::Error>(())
//! ```

use std::sync::{Arc, Mutex, Weak};
use std::thread;
use std::time::Duration;

use crate::config::{
    ADC_MAX, BLOCK_LEN, CAL_MAX, NUM_CHANNELS, NUM_PANELS, PadConfig, raw_to_calibrated,
};
use crate::context::{Context, SCAN_INTERVAL};
use crate::error::Result;
use crate::fake::{FakeBus, FakeMaster};

/// Each answering panel sends a sample this often (50 Hz), between the
/// master's default (~13 Hz per panel) and its fastest (~113 Hz).
const SAMPLE_INTERVAL: Duration = Duration::from_millis(20);

/// How a simulated pad is set up.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SimPad {
    /// The master's player-ID DIP: 0–3 for P1–P4. 4–7 are refused, as a real
    /// master's would be.
    pub player: u8,
    /// High Speed (1024-byte packets to the master) or Full Speed (64).
    pub high_speed: bool,
    /// Bit n = panel n answers the master.
    pub panels: u16,
    /// Per panel, bit n = a sensor is fitted to channel n. The config's
    /// `populated` flags are separate, so the two can disagree, as on a real
    /// pad.
    pub sensors: [u8; NUM_PANELS],
    /// The master found no valid saved config at boot and runs on defaults.
    pub config_was_reset: bool,
}

impl Default for SimPad {
    fn default() -> Self {
        SimPad {
            player: 0,
            high_speed: true,
            panels: 0x1FF,
            sensors: [0x0F; NUM_PANELS],
            config_was_reset: false,
        }
    }
}

/// Simulated pads, plugged and unplugged at will. Cheap to clone; every clone
/// is the same set of pads. Samples stop when the last clone is dropped.
#[derive(Clone)]
pub struct Simulator {
    shared: Arc<Shared>,
}

struct Shared {
    bus: FakeBus,
    pads: Mutex<Vec<SimState>>,
}

struct SimState {
    pad: SimPad,
    path: String,
    plugged: bool,
    /// The master's saved config, kept while it is unplugged, as its EEPROM.
    config: [u8; BLOCK_LEN],
    /// Each channel's rest reading, which is also its tare.
    rest: [[u16; NUM_CHANNELS]; NUM_PANELS],
    /// Weight on each channel in calibrated units, 0–1000.
    load: [[u16; NUM_CHANNELS]; NUM_PANELS],
    /// Bit n = channel n pressed, after hysteresis.
    pressed: [u8; NUM_PANELS],
    noise: u32,
}

impl Default for Simulator {
    fn default() -> Self {
        Self::new()
    }
}

impl Simulator {
    pub fn new() -> Self {
        let shared = Arc::new(Shared {
            bus: FakeBus::new(),
            pads: Mutex::new(Vec::new()),
        });
        let weak = Arc::downgrade(&shared);
        thread::Builder::new()
            .name("krakenpad-sim".into())
            .spawn(move || sample_loop(weak))
            .expect("could not start the simulator thread");
        Simulator { shared }
    }

    /// A context that finds this simulator's pads instead of real ones.
    pub fn context(&self) -> Result<Context> {
        Context::with_opener(Box::new(self.shared.bus.opener()), SCAN_INTERVAL)
    }

    /// Plugs in a new pad. Two with one player ID are allowed, so an app can
    /// see the SDK refuse the second; the calls below act on the first.
    pub fn plug(&self, pad: SimPad) {
        let mut pads = self.shared.pads.lock().unwrap();
        let n = pads.len();
        let mut s = SimState {
            pad,
            path: format!("sim:{n}"),
            plugged: false,
            config: PadConfig::default().to_bytes(),
            rest: [[0; NUM_CHANNELS]; NUM_PANELS],
            load: [[0; NUM_CHANNELS]; NUM_PANELS],
            pressed: [0; NUM_PANELS],
            noise: 0x9E37_79B9 ^ (n as u32 + 1).wrapping_mul(0x85EB_CA6B),
        };
        // Rest readings in the ranges measured on rev 1 (`docs/BRINGUP_LOG.md`):
        // 88–114 for a sensor in the pad, 13–18 for an empty socket.
        for p in 0..NUM_PANELS {
            for ch in 0..NUM_CHANNELS {
                let r = s.random();
                s.rest[p][ch] = if pad.sensors[p] >> ch & 1 != 0 {
                    88 + (r % 27) as u16
                } else {
                    13 + (r % 6) as u16
                };
            }
        }
        self.shared.connect(&mut s);
        pads.push(s);
    }

    /// The player IDs of every simulated pad, plugged or not, in the order
    /// they were added.
    pub fn players(&self) -> Vec<u8> {
        let pads = self.shared.pads.lock().unwrap();
        pads.iter().map(|s| s.pad.player).collect()
    }

    /// The pad's setup as it is now (panels answering included), and whether
    /// it is plugged in.
    pub fn pad(&self, player: u8) -> Option<(SimPad, bool)> {
        let pads = self.shared.pads.lock().unwrap();
        pads.iter()
            .find(|s| s.pad.player == player)
            .map(|s| (s.pad, s.plugged))
    }

    /// Unplugs or replugs a pad's USB. Its saved config survives, as a real
    /// master's EEPROM does.
    pub fn set_plugged(&self, player: u8, plugged: bool) {
        let mut pads = self.shared.pads.lock().unwrap();
        let Some(s) = pads.iter_mut().find(|s| s.pad.player == player) else {
            return;
        };
        if plugged && !s.plugged {
            self.shared.connect(s);
        } else if !plugged && s.plugged {
            s.config = self.shared.bus.with(&s.path, |m| m.config);
            self.shared.bus.unplug(&s.path);
            s.plugged = false;
        }
    }

    /// Makes a panel answer the master or go quiet, as with its 12 V or bus
    /// cable.
    pub fn set_panel_present(&self, player: u8, panel: usize, present: bool) {
        let mut pads = self.shared.pads.lock().unwrap();
        let Some(s) = pads.iter_mut().find(|s| s.pad.player == player) else {
            return;
        };
        s.pad.panels = if present {
            s.pad.panels | 1 << panel
        } else {
            s.pad.panels & !(1 << panel)
        };
        if s.plugged {
            self.shared
                .bus
                .with(&s.path, |m| m.presence(panel as u8, present));
        }
    }

    /// The weight on one panel's sensors, per channel in calibrated units
    /// (0 = resting, 1000 = full scale). An empty socket ignores it.
    pub fn set_load(&self, player: u8, panel: usize, load: [u16; NUM_CHANNELS]) {
        let mut pads = self.shared.pads.lock().unwrap();
        if let Some(s) = pads.iter_mut().find(|s| s.pad.player == player) {
            s.load[panel] = load.map(|v| v.min(CAL_MAX));
        }
    }
}

impl Shared {
    fn connect(&self, s: &mut SimState) {
        let master = FakeMaster {
            player: s.pad.player,
            present: s.pad.panels,
            was_reset: s.pad.config_was_reset,
            high_speed: s.pad.high_speed,
            config: s.config,
            ..Default::default()
        };
        self.bus.plug(&s.path, master);
        s.plugged = true;
    }

    /// One sample from every answering panel of every plugged pad.
    fn sample(&self) {
        let mut pads = self.pads.lock().unwrap();
        for s in pads.iter_mut().filter(|s| s.plugged) {
            let path = s.path.clone();
            self.bus.with(&path, |m| {
                // Once the host saves a config, a reset is behind it.
                s.pad.config_was_reset = m.was_reset;
                let config = PadConfig::from_bytes(&m.config).unwrap_or_default();
                let present = m.present;
                for p in (0..NUM_PANELS).filter(|p| present >> p & 1 != 0) {
                    let mut raw = [0u16; NUM_CHANNELS];
                    for (ch, r) in raw.iter_mut().enumerate() {
                        *r = s.reading(p, ch);
                        let c = config.panels[p].channels[ch];
                        let cal = raw_to_calibrated(*r, s.rest[p][ch]);
                        let bit = 1u8 << ch;
                        let was = s.pressed[p] & bit != 0;
                        let now = c.detects_presses()
                            && if was {
                                cal > c.release as i16
                            } else {
                                cal >= c.press as i16
                            };
                        s.pressed[p] = if now {
                            s.pressed[p] | bit
                        } else {
                            s.pressed[p] & !bit
                        };
                    }
                    m.stream(p as u8, raw, s.rest[p], s.pressed[p]);
                }
            });
        }
    }
}

impl SimState {
    /// xorshift32: enough for noise.
    fn random(&mut self) -> u32 {
        let mut x = self.noise;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        self.noise = x;
        x
    }

    fn reading(&mut self, panel: usize, ch: usize) -> u16 {
        let rest = self.rest[panel][ch] as i32;
        let fitted = self.pad.sensors[panel] >> ch & 1 != 0;
        let (signal, spread) = if fitted {
            let span = ADC_MAX as i32 - rest;
            (self.load[panel][ch] as i32 * span / CAL_MAX as i32, 5)
        } else {
            (0, 3)
        };
        let noise = (self.random() % spread) as i32 - (spread as i32 / 2);
        (rest + signal + noise).clamp(0, ADC_MAX as i32) as u16
    }
}

fn sample_loop(weak: Weak<Shared>) {
    while let Some(shared) = weak.upgrade() {
        shared.sample();
        drop(shared);
        thread::sleep(SAMPLE_INTERVAL);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Event;

    fn connected(ctx: &Context) -> u8 {
        loop {
            match ctx.wait_event(Duration::from_secs(5)) {
                Some(Event::PadConnected { player }) => return player,
                Some(_) => {}
                None => panic!("no pad connected"),
            }
        }
    }

    #[test]
    fn a_held_panel_presses_and_releases_by_the_config() {
        let sim = Simulator::new();
        sim.plug(SimPad::default());
        let ctx = sim.context().unwrap();
        let pad = ctx.pad(connected(&ctx)).unwrap();
        pad.set_telemetry(true).unwrap();
        let sample = |pad: &crate::Pad| {
            thread::sleep(SAMPLE_INTERVAL * 5);
            pad.snapshot().panels[4].sample.expect("C streams")
        };

        assert_eq!(sample(&pad).pressed, 0);
        sim.set_load(0, 4, [1000, 1000, 0, 0]);
        assert_eq!(sample(&pad).pressed, 0b0011);
        // Between the release (905) and the press (930): stays pressed.
        sim.set_load(0, 4, [920, 920, 0, 0]);
        assert_eq!(sample(&pad).pressed, 0b0011);
        sim.set_load(0, 4, [0; 4]);
        assert_eq!(sample(&pad).pressed, 0);
    }

    #[test]
    fn a_quiet_panel_and_an_unplug_reach_the_sdk() {
        let sim = Simulator::new();
        sim.plug(SimPad::default());
        let ctx = sim.context().unwrap();
        let player = connected(&ctx);
        sim.set_panel_present(player, 8, false);
        let mut saw_presence = false;
        while let Some(e) = ctx.wait_event(Duration::from_secs(2)) {
            if e == (Event::PanelPresence {
                player,
                panel: 8,
                present: false,
            }) {
                saw_presence = true;
                break;
            }
        }
        assert!(saw_presence);
        sim.set_plugged(player, false);
        loop {
            match ctx.wait_event(Duration::from_secs(5)) {
                Some(Event::PadDisconnected { player: p }) if p == player => break,
                Some(_) => {}
                None => panic!("no disconnect"),
            }
        }
    }
}
