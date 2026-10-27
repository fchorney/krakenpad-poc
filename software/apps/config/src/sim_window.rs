//! The simulator's controls (`--sim` only), in a window of their own so they
//! stay up on every page: plug and unplug pads, make panels go quiet, and
//! press panels with the mouse.

use eframe::egui::{self, RichText};
use krakenpad::sim::Simulator;

use crate::overview::NAMES;

pub struct SimWindow {
    pub sim: Simulator,
    pub open: bool,
    /// Weight a held panel gets on every sensor, in calibrated units.
    weight: u16,
    /// The (player, panel) being held down with the mouse.
    held: Option<(u8, usize)>,
}

impl SimWindow {
    pub fn new(sim: Simulator) -> Self {
        SimWindow {
            sim,
            open: true,
            weight: 1000,
            held: None,
        }
    }

    pub fn show(&mut self, ctx: &egui::Context) {
        let mut open = self.open;
        let mut held = None;
        egui::Window::new("Simulator")
            .open(&mut open)
            .resizable(false)
            // Bottom right, clear of the pad cards.
            .pivot(egui::Align2::RIGHT_BOTTOM)
            .default_pos(ctx.content_rect().right_bottom() - egui::vec2(12.0, 12.0))
            .show(ctx, |ui| {
                ui.label("Hold a panel to press it. Right-click: it stops or starts answering.");
                ui.add(
                    egui::Slider::new(&mut self.weight, 0..=1000).text("weight on a held panel"),
                );
                ui.add_space(6.0);
                ui.horizontal_top(|ui| {
                    for player in self.sim.players() {
                        let Some((pad, plugged)) = self.sim.pad(player) else {
                            continue;
                        };
                        ui.vertical(|ui| {
                            let mut p = plugged;
                            if ui
                                .checkbox(&mut p, format!("P{} plugged in", player + 1))
                                .changed()
                            {
                                self.sim.set_plugged(player, p);
                            }
                            egui::Grid::new(("sim-grid", player))
                                .spacing([4.0, 4.0])
                                .show(ui, |ui| {
                                    for (i, name) in NAMES.iter().enumerate() {
                                        let present = pad.panels >> i & 1 != 0;
                                        let label = if present {
                                            RichText::new(*name)
                                        } else {
                                            RichText::new(*name).weak().strikethrough()
                                        };
                                        let b = ui.add_enabled(
                                            plugged,
                                            egui::Button::new(label).min_size([36.0, 36.0].into()),
                                        );
                                        if b.is_pointer_button_down_on()
                                            && ui.input(|i| i.pointer.primary_down())
                                        {
                                            held = Some((player, i));
                                        }
                                        if b.secondary_clicked() {
                                            self.sim.set_panel_present(player, i, !present);
                                        }
                                        if i % 3 == 2 {
                                            ui.end_row();
                                        }
                                    }
                                });
                        });
                        ui.add_space(12.0);
                    }
                });
            });
        self.open = open;

        if held != self.held {
            if let Some((player, panel)) = self.held {
                self.sim.set_load(player, panel, [0; 4]);
            }
            self.held = held;
        }
        if let Some((player, panel)) = held {
            self.sim.set_load(player, panel, [self.weight; 4]);
        }
    }
}
