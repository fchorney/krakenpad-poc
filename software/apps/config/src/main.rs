//! The KrakenPad config app (`docs/HOST_SOFTWARE.md` → "Config app").
//!
//!     cargo run -p krakenpad-config                (from software/)
//!     cargo run -p krakenpad-config -- --sim       (simulated pads, no hardware)

mod app;
mod overview;
mod sim_window;
mod worker;

use eframe::egui;
use krakenpad::sim::{SimPad, Simulator};

fn main() -> eframe::Result {
    let mut sim = None;
    for arg in std::env::args().skip(1) {
        match arg.as_str() {
            "--sim" => sim = Some(demo_pads()),
            _ => {
                eprintln!("usage: krakenpad-config [--sim]");
                std::process::exit(2);
            }
        }
    }
    // Before the event loop starts: on macOS, hidapi's enumeration runs the
    // calling thread's run loop, and from inside winit's event handling (where
    // eframe builds the app) that re-enters winit, which panics.
    let sdk = match &sim {
        Some(s) => s.context(),
        None => krakenpad::Context::new(),
    };
    let title = if sim.is_some() {
        "KrakenPad Config (simulated pads)"
    } else {
        "KrakenPad Config"
    };
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title(title)
            .with_inner_size([1200.0, 760.0]),
        ..Default::default()
    };
    eframe::run_native(
        "KrakenPad Config",
        options,
        Box::new(|cc| Ok(Box::new(app::App::new(cc, sdk, sim)))),
    )
}

/// Two pads to build the window against: P1 as it should be, and P2 with
/// things to show: Full Speed, DR not answering, sensors only on the five
/// panels the bench pad has them on, and a master that lost its config.
fn demo_pads() -> Simulator {
    let sim = Simulator::new();
    sim.plug(SimPad::default());
    let mut sensors = [0; 9];
    for p in [1, 3, 4, 5, 7] {
        sensors[p] = 0x0F;
    }
    sim.plug(SimPad {
        player: 1,
        high_speed: false,
        panels: 0x0FF,
        sensors,
        config_was_reset: true,
    });
    sim
}
