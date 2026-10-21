//! Bench check of the SDK against a real pad: prints every event, turns on
//! telemetry for each pad that connects, and twice a second prints each
//! panel's raw FSR values (`*` = pressed, `!` = not configured).
//!
//!     cargo run --example monitor        (from software/; Ctrl-C to stop)

use std::time::{Duration, Instant};

use krakenpad::{Context, Event};

const NAMES: [&str; 9] = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"];

fn main() -> krakenpad::Result<()> {
    let ctx = Context::new()?;
    println!(
        "krakenpad {}: waiting for pads, Ctrl-C to stop",
        krakenpad::VERSION
    );
    let mut last_print = Instant::now();
    loop {
        if let Some(e) = ctx.wait_event(Duration::from_millis(100)) {
            println!("event: {e:?}");
            if let Event::PadConnected { player } = e {
                let pad = ctx.pad(player).expect("just connected");
                let i = pad.info();
                let (major, minor, patch) = i.firmware_version;
                println!(
                    "  P{}: master {major}.{minor}.{patch} build {:08X}, panels {:09b}, termination {:?}",
                    player + 1,
                    i.build_tag,
                    i.panels_present,
                    i.termination
                );
                println!(
                    "  USB {:?} speed, packets {} B to the master, {} B back",
                    i.usb_speed, i.out_packet, i.in_packet
                );
                if let Err(err) = pad.set_telemetry(true) {
                    println!("  telemetry: {err}");
                }
            }
        }
        if last_print.elapsed() >= Duration::from_millis(500) {
            last_print = Instant::now();
            for pad in ctx.pads() {
                let s = pad.snapshot();
                let panels: Vec<String> = s
                    .panels
                    .iter()
                    .zip(NAMES)
                    .filter_map(|(p, name)| {
                        let x = p.sample?;
                        let raw = x.raw.map(|v| v.to_string()).join(",");
                        let pressed = if x.pressed != 0 { "*" } else { "" };
                        let unconfigured = if x.configured { "" } else { "!" };
                        Some(format!("{name}:{raw}{pressed}{unconfigured}"))
                    })
                    .collect();
                if !panels.is_empty() {
                    println!("P{} {}", s.player + 1, panels.join("  "));
                }
            }
        }
    }
}
