//! Overview: every connected pad side by side, with its master, its USB link
//! and its panels.

use eframe::egui::{self, Align2, Color32, FontId, RichText, Sense, Stroke, StrokeKind, Vec2};
use krakenpad::{ConfigState, NUM_PANELS, Pad, PadSnapshot, PadStatus, Termination, UsbSpeed};

use crate::worker::Shared;

/// Panel names by panel ID, which is also their place in the 3×3 grid.
pub const NAMES: [&str; NUM_PANELS] = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"];

const CARD_WIDTH: f32 = 330.0;
const CELL: f32 = 62.0;
const GAP: f32 = 6.0;

pub fn show(ui: &mut egui::Ui, pads: &[Pad], shared: &Shared) {
    ui.heading("Overview");
    ui.add_space(8.0);
    if pads.is_empty() {
        ui.label("No pads found. Plug in a master's USB; it shows up within a second.");
    }
    ui.horizontal_wrapped(|ui| {
        ui.spacing_mut().item_spacing = Vec2::splat(12.0);
        for pad in pads {
            let snap = pad.snapshot();
            card(
                ui,
                &snap,
                shared.status.get(&snap.player),
                shared.errors.get(&snap.player),
            );
        }
    });
    ui.add_space(12.0);
    egui::CollapsingHeader::new("Event log").show(ui, |ui| {
        egui::ScrollArea::vertical()
            .max_height(220.0)
            .stick_to_bottom(true)
            .auto_shrink([false, true])
            .show(ui, |ui| {
                for (t, e) in &shared.events {
                    ui.monospace(format!("{t:7.2} s  {e:?}"));
                }
            });
    });
}

fn card(ui: &mut egui::Ui, s: &PadSnapshot, status: Option<&PadStatus>, error: Option<&String>) {
    egui::Frame::group(ui.style())
        .inner_margin(12.0)
        .show(ui, |ui| {
            // Top-down inside the frame: it would inherit the wrapping row.
            ui.vertical(|ui| card_body(ui, s, status, error));
        });
}

fn card_body(
    ui: &mut egui::Ui,
    s: &PadSnapshot,
    status: Option<&PadStatus>,
    error: Option<&String>,
) {
    ui.set_width(CARD_WIDTH);
    ui.spacing_mut().item_spacing.y = 4.0;
    let i = s.info;
    let warn = ui.visuals().warn_fg_color;

    ui.label(
        RichText::new(format!("P{}", s.player + 1))
            .heading()
            .strong(),
    );
    let (major, minor, patch) = i.firmware_version;
    ui.label(format!(
        "Master {major}.{minor}.{patch} · build {:08X}",
        i.build_tag
    ));
    match i.usb_speed {
        UsbSpeed::High => {
            ui.label("USB High Speed · joystick at 8 kHz");
        }
        UsbSpeed::Full => {
            ui.label("USB Full Speed · joystick at 1 kHz");
            // Measured on master #2, 2026-10-21 (docs/USB_PROTOCOL.md).
            ui.label(
                RichText::new("An LED frame from this PC takes ~15 ms (0.7 ms at High Speed)")
                    .small()
                    .color(warn),
            );
        }
        UsbSpeed::Other(c) => {
            ui.label(format!("USB speed code {c}"));
        }
    }
    ui.label(
        RichText::new(format!(
            "Packets: {} B to the master, {} B back",
            i.out_packet, i.in_packet
        ))
        .small()
        .weak(),
    );

    ui.add_space(6.0);
    panel_grid(ui, s, status);
    ui.add_space(6.0);

    let missing: Vec<&str> = (0..NUM_PANELS)
        .filter(|&p| !s.panels[p].present)
        .map(|p| NAMES[p])
        .collect();
    if missing.is_empty() {
        ui.label("All 9 panels answering");
    } else {
        ui.colored_label(
            warn,
            format!(
                "{} of 9 panels answering · not: {}",
                9 - missing.len(),
                missing.join(" ")
            ),
        );
    }
    match i.termination {
        Termination::Ok => {
            let name = i
                .terminated_panel
                .map_or("?", |p| NAMES[p as usize % NUM_PANELS]);
            ui.label(format!("Termination: {name}, the last panel in the chain"));
        }
        Termination::NoneTerminated => {
            ui.colored_label(warn, "No answering panel is terminated");
        }
        Termination::Several => {
            ui.colored_label(warn, "More than one panel is terminated");
        }
        Termination::WrongPanel => {
            ui.colored_label(warn, "The terminated panel isn't the last in the chain");
        }
        Termination::Unknown => {
            ui.weak("Termination: not checked yet");
        }
        Termination::Other(c) => {
            ui.colored_label(warn, format!("Termination code {c}"));
        }
    }
    if i.config_was_reset {
        ui.colored_label(
            warn,
            "The master found no saved config and runs on defaults: recalibrate",
        );
    }
    if i.unsaved_edits {
        ui.colored_label(warn, "Unsaved config edits from the master's console");
    }
    if i.passthrough {
        ui.colored_label(
            warn,
            "Bus passthrough active: a panel update may be running",
        );
    }
    if let Some(e) = error {
        ui.colored_label(ui.visuals().error_fg_color, e);
    }
}

/// The 3×3 panels: lit while pressed, dimmed while not answering, outlined in
/// the warning colour if the panel's config doesn't match the master's.
/// Hovering one shows its details.
fn panel_grid(ui: &mut egui::Ui, s: &PadSnapshot, status: Option<&PadStatus>) {
    let side = 3.0 * CELL + 2.0 * GAP;
    let (grid, _) = ui.allocate_exact_size(Vec2::splat(side), Sense::hover());
    let v = ui.visuals().clone();
    for (p, name) in NAMES.iter().enumerate() {
        let (row, col) = ((p / 3) as f32, (p % 3) as f32);
        let min = grid.min + Vec2::new(col * (CELL + GAP), row * (CELL + GAP));
        let rect = egui::Rect::from_min_size(min, Vec2::splat(CELL));
        let panel = &s.panels[p];
        let detail = status.map(|st| st.panels[p]);
        let pressed = panel.present && panel.sample.is_some_and(|x| x.pressed != 0);
        let config = detail.map(|d| d.config_state).or(panel.config_state);
        let config_ok = matches!(config, None | Some(ConfigState::Matches));

        let (fill, text) = if !panel.present {
            (Color32::TRANSPARENT, v.weak_text_color())
        } else if pressed {
            (v.selection.bg_fill, v.strong_text_color())
        } else {
            (v.widgets.inactive.bg_fill, v.text_color())
        };
        let stroke = if !panel.present {
            Stroke::new(1.0, v.weak_text_color())
        } else if !config_ok {
            Stroke::new(2.0, v.warn_fg_color)
        } else {
            v.widgets.inactive.bg_stroke
        };
        let painter = ui.painter();
        painter.rect_filled(rect, 6.0, fill);
        painter.rect_stroke(rect, 6.0, stroke, StrokeKind::Inside);
        painter.text(
            rect.center(),
            Align2::CENTER_CENTER,
            name,
            FontId::proportional(16.0),
            text,
        );
        if detail.and_then(|d| d.terminated) == Some(true) {
            painter.text(
                rect.right_bottom() + Vec2::new(-5.0, -3.0),
                Align2::RIGHT_BOTTOM,
                "T",
                FontId::proportional(11.0),
                v.weak_text_color(),
            );
        }

        let id = egui::Id::unique(("overview-panel", s.player, p));
        ui.interact(rect, id, Sense::hover()).on_hover_ui(|ui| {
            ui.strong(*name);
            ui.label(if panel.present {
                "Answering the master"
            } else {
                "Not answering the master"
            });
            if let Some(d) = detail {
                if let Some(age) = d.last_reply {
                    ui.label(format!("Last reply {} ms ago", age.as_millis()));
                }
                ui.label(format!("Polls answered: {}%", d.polls_answered_percent));
                ui.label(format!("CRC errors: {}", d.crc_errors));
                ui.label(format!("Config: {}", config_text(d.config_state)));
                if let Some(tag) = d.build_tag {
                    ui.label(format!("Firmware build {tag:08X}"));
                }
                if let Some(t) = d.terminated {
                    ui.label(format!(
                        "Termination switch {}",
                        if t { "on" } else { "off" }
                    ));
                }
            }
        });
    }
}

fn config_text(c: ConfigState) -> String {
    match c {
        ConfigState::Matches => "matches the master's".into(),
        ConfigState::Waiting => "sent, not confirmed yet".into(),
        ConfigState::RejectedVersion => "refused: the panel's firmware needs updating".into(),
        ConfigState::RejectedInvalid => "refused as invalid".into(),
        ConfigState::NotConfigured => "not sent yet since the panel booted".into(),
        ConfigState::Absent => "panel absent".into(),
        ConfigState::Other(c) => format!("state code {c}"),
    }
}
