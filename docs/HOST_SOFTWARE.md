# Host software — SDK and apps

**Status: decided, not built** (2026-10-20). What runs on the PC: the SDK that
games link, and the apps built on it. The USB interface they talk to is
`docs/USB_PROTOCOL.md`; today only its joystick report is built.

## Decisions

**DECIDED 2026-10-20: one SDK, written in Rust, with a C interface.**
- The main first user is deadsync, which is written in Rust, so it gets a native
  crate.
- C and C++ programs (ITGmania, the C++ editor in the meantime) link the same
  library through a C header generated from the Rust code (`cbindgen`), as a
  static or shared library.
- **Why not two ports** (Rust and C++, as with the stock-SMX SDK fork): one
  implementation means deadsync and ITGmania can never disagree about the
  protocol. Why not a C/C++ core with Rust bindings: Rust bindings over C code
  were the less pleasant direction when looked at before, and they would put a
  C++ toolchain into deadsync's build.
- **Cost:** a C/C++ program that builds the SDK from source needs a Rust
  toolchain. ITGmania vendors its libraries when needed, so a vendored build or
  a prebuilt library per platform is the expected route there.
- **Fallback, if a C++ project won't take a Rust dependency:** a hand-written
  C++ port. Then `docs/USB_PROTOCOL.md` is the contract both implementations are
  tested against, with shared test data (recorded reports and their decoded
  values) so they can't drift apart quietly.

**DECIDED 2026-10-20: USB HID through hidapi**, which deadsync and ITGmania
already use.

**DECIDED 2026-10-20: the apps are Rust + egui** (via eframe): the config app,
the animation editor and the updater.
- Every app runs on Windows, macOS and Linux, as one codebase.
- egui is immediate-mode like Dear ImGui, which `stepmaniax-gif-maker` uses, so
  that editor's layout and interaction patterns carry over.
- Considered: Slint (licence split to check), Iced (maturing), Tauri (a web UI
  in three different system webviews; poor fit for a pixel editor).
- Until the Rust editor exists, the C++ `stepmaniax-gif-maker` fork remains the
  starting template (`docs/ANIMATIONS.md`) and can use the SDK through its C
  interface.

**DECIDED 2026-10-20: the bench tools in `tools/` stay Python.** Changing their
language buys nothing. `panel_update.py` and `panel_boot_update.py` keep their
own protocol code until the SDK can do the same job.

**DECIDED 2026-10-20: everything lives in this repo for now**: SDK, apps,
firmware, tools. This repo is the prototype, where the decisions get made.
Once the user calls rev 1's software and firmware finished, each element moves
to its own repo. Until then the SDK is **not published to crates.io**; deadsync
uses it as a local path dependency when it gets there.

## Not decided

- Crate and library names, and the C interface's shape (handles, callbacks vs
  polling for input and telemetry).
- Versions to pin. egui, eframe and the hidapi crate were chosen from what was
  known on 2026-10-20; check current releases and maintenance before starting.
- How the SDK takes over bus updates (`docs/FIELD_UPDATE.md`), which today go
  through the master's text-console passthrough rather than a USB report.
