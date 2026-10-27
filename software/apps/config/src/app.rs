//! The window: a sidebar of sections, and the chosen section beside it.

use std::sync::{Arc, Mutex};
use std::time::Duration;

use eframe::egui::{self, RichText};
use krakenpad::sim::Simulator;

use crate::sim_window::SimWindow;
use crate::{overview, worker};

#[derive(Clone, Copy, PartialEq, Eq)]
enum Page {
    Overview,
    Sensors,
    Lights,
    Updates,
}

const PAGES: [(Page, &str); 4] = [
    (Page::Overview, "Overview"),
    (Page::Sensors, "Sensors"),
    (Page::Lights, "Lights"),
    (Page::Updates, "Updates"),
];

/// Live telemetry has no event to wake the window, so while a pad is
/// connected it redraws at this interval.
const LIVE_REDRAW: Duration = Duration::from_millis(33);

pub struct App {
    /// An error here means the SDK wouldn't start; the window says so.
    sdk: Result<Arc<krakenpad::Context>, String>,
    shared: Arc<Mutex<worker::Shared>>,
    page: Page,
    sim: Option<SimWindow>,
    /// Dev aid: `KRAKENPAD_SCREENSHOT=out.bmp` saves the window once it has
    /// settled, then quits, for checking a layout without the screen.
    screenshot: Option<String>,
}

impl App {
    pub fn new(
        cc: &eframe::CreationContext<'_>,
        sdk: krakenpad::Result<krakenpad::Context>,
        sim: Option<Simulator>,
    ) -> Self {
        let shared = Arc::new(Mutex::new(worker::Shared::default()));
        let sdk = sdk.map(Arc::new).map_err(|e| e.to_string());
        if let Ok(sdk) = &sdk {
            worker::spawn(Arc::downgrade(sdk), shared.clone(), cc.egui_ctx.clone());
        }
        App {
            sdk,
            shared,
            page: Page::Overview,
            sim: sim.map(SimWindow::new),
            screenshot: std::env::var("KRAKENPAD_SCREENSHOT").ok(),
        }
    }
}

impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        egui::Panel::left("sections")
            .resizable(false)
            .exact_size(150.0)
            .show(ui, |ui| {
                ui.add_space(10.0);
                ui.label(RichText::new("KrakenPad").heading().strong());
                ui.weak(format!("SDK {}", krakenpad::VERSION));
                if self.sim.is_some() {
                    ui.colored_label(ui.visuals().warn_fg_color, "Simulated pads");
                }
                ui.add_space(10.0);
                for (page, name) in PAGES {
                    let b = egui::Button::selectable(self.page == page, name)
                        .min_size([ui.available_width(), 28.0].into());
                    if ui.add(b).clicked() {
                        self.page = page;
                    }
                }
                if let Some(sim) = &mut self.sim {
                    ui.with_layout(egui::Layout::bottom_up(egui::Align::Min), |ui| {
                        ui.add_space(10.0);
                        ui.checkbox(&mut sim.open, "Simulator controls");
                    });
                }
            });

        egui::CentralPanel::default().show(ui, |ui| match &self.sdk {
            Err(e) => {
                ui.colored_label(
                    ui.visuals().error_fg_color,
                    format!("The SDK didn't start: {e}"),
                );
            }
            Ok(sdk) => {
                let pads = sdk.pads();
                if !pads.is_empty() {
                    ui.ctx().request_repaint_after(LIVE_REDRAW);
                }
                egui::ScrollArea::vertical().show(ui, |ui| match self.page {
                    Page::Overview => overview::show(ui, &pads, &self.shared.lock().unwrap()),
                    Page::Sensors => {
                        not_built(ui, "Sensors", "Live sensors and thresholds, with groups.")
                    }
                    Page::Lights => not_built(ui, "Lights", "LED modes and animations."),
                    Page::Updates => not_built(ui, "Updates", "Master and panel firmware."),
                });
            }
        });

        if let Some(sim) = &mut self.sim {
            sim.show(ui.ctx());
        }

        if self.screenshot.is_some() {
            if ui.input(|i| i.time) < 2.0 {
                ui.ctx().request_repaint_after(Duration::from_millis(100));
            } else if let Some(path) = self.screenshot.take() {
                let ctx = ui.ctx().clone();
                ui.ctx().request_screenshot(move |image| {
                    if let Err(e) = write_bmp(&path, &image) {
                        eprintln!("screenshot to {path}: {e}");
                    }
                    ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                });
            }
        }
    }
}

/// A 32-bit top-down BMP: no image crate for one dev aid.
fn write_bmp(path: &str, image: &egui::ColorImage) -> std::io::Result<()> {
    let [w, h] = image.size;
    let data = (w * h * 4) as u32;
    let mut out = Vec::with_capacity(54 + data as usize);
    out.extend(b"BM");
    out.extend((54 + data).to_le_bytes());
    out.extend([0; 4]);
    out.extend(54u32.to_le_bytes());
    out.extend(40u32.to_le_bytes());
    out.extend((w as i32).to_le_bytes());
    out.extend((-(h as i32)).to_le_bytes());
    out.extend(1u16.to_le_bytes());
    out.extend(32u16.to_le_bytes());
    out.extend(0u32.to_le_bytes());
    out.extend(data.to_le_bytes());
    out.extend([0; 16]);
    for c in &image.pixels {
        out.extend([c.b(), c.g(), c.r(), 255]);
    }
    std::fs::write(path, out)
}

fn not_built(ui: &mut egui::Ui, title: &str, what: &str) {
    ui.heading(title);
    ui.add_space(8.0);
    ui.label(what);
    ui.weak("Not built yet.");
}
