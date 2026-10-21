//! The KrakenPad SDK: telemetry, config and control for KrakenPad dance pads,
//! over the master's USB vendor interface (`docs/USB_PROTOCOL.md`).
//!
//! Gameplay presses don't go through here: they arrive as the standard
//! joystick report that games already read. The SDK adds what that can't
//! carry.
//!
//! - One [`Context`] per program manages every pad, up to four, keyed by the
//!   master's player-ID DIP. A background thread finds pads as they are
//!   plugged in; it never runs the program's code.
//! - **State** (presence, the latest FSR values, config) is a snapshot copied
//!   out whenever wanted: [`Pad::snapshot`].
//! - **Events that must not be missed** go into one queue, read by polling
//!   ([`Context::poll_event`]), by a blocking wait on a thread of the
//!   program's own ([`Context::wait_event`]), or after a wake-up call
//!   ([`Context::set_wake`]).
//! - Commands ([`Pad::get_config`] and the rest) block the calling thread
//!   until the master answers.
//!
//! ```no_run
//! use std::time::Duration;
//! use krakenpad::{Context, Event};
//!
//! let ctx = Context::new()?;
//! while let Some(event) = ctx.wait_event(Duration::from_secs(10)) {
//!     if let Event::PadConnected { player } = event {
//!         let pad = ctx.pad(player).expect("just connected");
//!         pad.set_telemetry(true)?;
//!         println!("P{}: {:?}", player + 1, pad.snapshot().panels[4].sample);
//!     }
//! }
//! # Ok::<(), krakenpad::Error>(())
//! ```
//!
//! Status: a skeleton (2026-10-20), tested against a fake master only; it has
//! not talked to a real pad yet. `docs/HOST_SOFTWARE.md` → "Built".

#![forbid(unsafe_code)]

mod config;
mod context;
mod error;
mod events;
#[cfg(test)]
mod fake;
mod messages;
mod pad;
mod protocol;
mod transport;

pub use config::{
    ADC_MAX, CONFIG_VERSION, ChannelConfig, DEFAULT_PRESS, DEFAULT_RELEASE, NUM_CHANNELS,
    NUM_PANELS, PadConfig, PanelConfig,
};
pub use context::Context;
pub use error::{DeviceError, Error, Result};
pub use events::{Event, RejectReason};
pub use messages::{
    ClockProbe, ConfigState, DeviceInfo, FsrSample, LedFrame, LedMode, PadStatus, PanelStatus,
    SelfTest, Termination, UsbSpeed,
};
pub use pad::{Pad, PadSnapshot, PadStats, PanelSnapshot};

/// This SDK's version.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
