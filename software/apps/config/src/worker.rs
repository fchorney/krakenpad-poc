//! The app's SDK thread. It reads the SDK's events and runs the commands that
//! block (telemetry on at connect, device info and pad status every few
//! seconds), so the window never waits on a master. The window reads what it
//! leaves in [`Shared`].

use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, Weak};
use std::thread;
use std::time::{Duration, Instant};

use eframe::egui;
use krakenpad::{Context, Event, PadStatus};

/// Pad status and device info are asked for this often, and sooner after an
/// event that changes them.
const STATUS_INTERVAL: Duration = Duration::from_secs(2);
const STATUS_AFTER_EVENT: Duration = Duration::from_millis(100);

/// Events kept for the log; older ones are dropped.
const EVENT_LOG_LEN: usize = 200;

#[derive(Default)]
pub struct Shared {
    /// (seconds since start, event), oldest first.
    pub events: Vec<(f32, Event)>,
    /// The latest pad status, by player.
    pub status: BTreeMap<u8, PadStatus>,
    /// The latest failed command, by player.
    pub errors: BTreeMap<u8, String>,
}

/// Runs until the window drops its [`Context`].
pub fn spawn(sdk: Weak<Context>, shared: Arc<Mutex<Shared>>, repaint: egui::Context) {
    thread::Builder::new()
        .name("config-sdk".into())
        .spawn(move || run(sdk, shared, repaint))
        .expect("could not start the SDK thread");
}

fn run(sdk: Weak<Context>, shared: Arc<Mutex<Shared>>, repaint: egui::Context) {
    let started = Instant::now();
    let mut next_status = Instant::now();
    while let Some(sdk) = sdk.upgrade() {
        let mut changed = false;
        if let Some(e) = sdk.wait_event(Duration::from_millis(200)) {
            changed = true;
            // Commands first: the window takes the lock every frame.
            let mut error = None;
            match e {
                Event::PadConnected { player } => {
                    // Telemetry is off at connect; the window shows it live.
                    if let Some(Err(err)) = sdk.pad(player).map(|p| p.set_telemetry(true)) {
                        error = Some((player, format!("telemetry: {err}")));
                    }
                    next_status = next_status.min(Instant::now() + STATUS_AFTER_EVENT);
                }
                Event::PanelPresence { .. } | Event::ConfigResult { .. } => {
                    next_status = next_status.min(Instant::now() + STATUS_AFTER_EVENT);
                }
                _ => {}
            }
            let mut sh = shared.lock().unwrap();
            if let Event::PadDisconnected { player } = e {
                sh.status.remove(&player);
                sh.errors.remove(&player);
            }
            if let Some((player, msg)) = error {
                sh.errors.insert(player, msg);
            }
            sh.events.push((started.elapsed().as_secs_f32(), e));
            let excess = sh.events.len().saturating_sub(EVENT_LOG_LEN);
            sh.events.drain(..excess);
        }
        if Instant::now() >= next_status {
            next_status = Instant::now() + STATUS_INTERVAL;
            for pad in sdk.pads() {
                let player = pad.player();
                // Device info refreshes the flags the window shows (config was
                // reset, unsaved console edits); pad status the panel details.
                let result = pad.device_info().and_then(|_| pad.pad_status());
                let mut sh = shared.lock().unwrap();
                match result {
                    Ok(s) => {
                        sh.status.insert(player, s);
                        sh.errors.remove(&player);
                    }
                    Err(err) => {
                        sh.errors.insert(player, format!("pad status: {err}"));
                    }
                }
            }
            changed = true;
        }
        drop(sdk);
        if changed {
            repaint.request_repaint();
        }
    }
}
