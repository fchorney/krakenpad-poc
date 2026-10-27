# Host software — SDK and apps

**Status: the SDK's Rust skeleton is built (2026-10-20) and runs against the
pad** (its `monitor` example, since the evening of 2026-10-20). **The config
app was started 2026-10-27**: a window that lists the pads and the SDK's
events ("Config app" below). The C interface and the other apps are not built.
What runs on the PC: the SDK that games link, and the apps
built on it. What exists: "Built (2026-10-20)" below. The USB interface they talk to is
`docs/USB_PROTOCOL.md`: the joystick report works on the bench, and the vendor
interface the SDK uses was built in the master 2026-10-20 and first ran on
master #2 the same evening. Until the SDK exists, **`tools/pad_cmd.py` speaks every vendor
command**, as a bench tool and as a working reference for the framing.

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

**DECIDED 2026-10-20: names and layout.** KrakenPad stays the name for now.

```
software/             Cargo workspace (separate from firmware/ and hardware/)
  sdk/                crate krakenpad: the SDK's Rust API (deadsync, our apps)
  sdk-c/              crate krakenpad-c: the C interface; builds libkrakenpad
                      (static + shared) and a generated krakenpad.h
  apps/config/        krakenpad-config
  apps/editor/        krakenpad-editor
```

- Two SDK crates so the Rust API never bends to C's limits and every `unsafe`
  line sits in the small `krakenpad-c`.
- C names are prefixed `krakenpad_` (types `Krakenpad…`): C has no namespaces,
  and a short prefix risks clashing inside a codebase the size of ITGmania's.
- **Updates live in the config app** (a tab or a "check for updates" dialog),
  not a separate app. Command-line tools for power users may follow if people
  want them.
- **The config app shows the USB connection** (the user, 2026-10-21): the
  negotiated speed, the report sizes, and what that allows in practice, such
  as the LED frame rate and telemetry rate the pad can hold on this computer.
  At Full Speed it says what is reduced (e.g. "LED frames limited to ~N fps").
  The numbers come from tests at both speeds, not yet run; whether Full Speed
  drops or degrades any feature is decided from those tests
  (`docs/USB_PROTOCOL.md` → packet size).

**DECIDED 2026-10-20: how the SDK delivers data.** The stock SDK is a template,
not a spec; this was chosen against the alternatives below.

- The SDK runs its own background thread reading HID. **It never runs game code
  on that thread.**
- **Current state** (connection, latest FSR values, config) is a snapshot the
  game copies out whenever it wants, from any thread.
- **Events that must not be missed** (connect/disconnect, config write done or
  failed, update progress, and later timestamped presses) go into one queue,
  which the game reads in any of three ways:
  - **B, poll:** drain it once per frame. Costs about one atomic read when empty.
  - **C, blocking wait:** `wait_event(timeout)` on a thread the game owns, which
    sleeps until something arrives. This is how a game keeps SDK work entirely
    off its main loop, e.g. its input thread.
  - **D, wake-up signal:** the SDK calls one tiny game-supplied function that
    only says "something is ready" (to post into an engine's own event loop);
    the game then pulls as in B.
- **Not offered, for now:** A, full callbacks with data on the SDK's thread (the
  stock SDK's model). They move the hand-off into every game and need rules like
  "return fast, don't call stop from inside"; they can be added if someone asks.
  E, Rust async streams: deadsync doesn't need them (below).
- **One context manages every pad**, as the stock SDK does, but **up to four**,
  keyed by the master's player-ID DIP (P1–P4; 4–7 reserved). Every snapshot and
  event names its pad.
- Gameplay presses keep arriving through the standard joystick HID that games
  already read. The SDK adds telemetry, config, updates, and later the
  timestamped input stream (`docs/TIMESTAMPED_INPUT.md`).

Defaults that came with it: opaque handles with open/close and a pad list;
every C function returns a status code (`krakenpad_error_string()` for text);
structs are filled into caller-provided memory, so the SDK never hands back
memory to free; `krakenpad_version()` and a size field leading each struct, so
a newer library works with an older game.

### How deadsync is built (read 2026-10-20, upstream `bacdc3e37`)

What the SDK has to fit, from deadsync's source:

- **Main loop: winit.** Input backends run on threads deadsync spawns, and post
  events into the main loop through winit's `EventLoopProxy`
  (`crates/deadsync-shell/src/input_backend.rs`). The stock-SMX support
  (`deadsync-smx`, on the `rustmaniax-sdk` crate) takes the SDK's callbacks and
  forwards them through that same proxy.
- **FSR telemetry is polled.** The FSR screens call `poll_pads_into()` and read
  a `PadView` snapshot per frame (`crates/deadsync-input-fsr/`), with one backend
  per pad family (`BackendKind::{Fsrio, Smx}`).
- **No async in the input path.** tokio appears only in the heart-rate crate, on
  its own runtime thread.

So deadsync would use the SDK as: a thread of its own blocking on the event
queue (C) that forwards into `EventLoopProxy`, like its native input backends;
and the state snapshot (B) behind a new `BackendKind::KrakenPad` for the FSR
screens. Rust async (E) isn't needed.

Constraints it puts on the SDK:
- deadsync supports **Windows 7** and patches the `hidapi` crate for it
  (`[patch.crates-io]` → `vendor/hidapi-win7`). The SDK must depend on the
  `hidapi` crate (not its own HID code), so that patch covers it too.
- deadsync builds with **edition 2024, `rust-version = 1.97`**, and pins
  `hidapi = 2.6.7` per OS (`windows-native`, `linux-static-hidraw`).

**DECIDED 2026-10-20: versions match deadsync where they overlap**, so the SDK
drops into deadsync without conflicts.

| | Pinned | Why |
|---|---|---|
| `hidapi` crate | **2.6.7**, `windows-native` on Windows, `linux-static-hidraw` on Linux | deadsync's version and backends; its Windows 7 patch then covers the SDK |
| Rust edition | **2024** | deadsync's |
| `rust-version` | **1.97**, fixed; raised only on purpose | deadsync's minimum today; the SDK must never be what raises deadsync's |
| `egui` / `eframe` | latest release when the apps are started, then pinned exactly: **`eframe` 0.37.0** (2026-10-27, the config app; needs Rust 1.95, inside the 1.97 pin) | apps only; not in the SDK or deadsync |
| `cbindgen` | latest when `krakenpad-c` is started | build-time tool only |

**DECIDED 2026-10-20: saving config and uploading animations are not for use
during gameplay.** Saving stalls the master (`docs/PANEL_CONFIG.md` → "Config
persistence"). The SDK can't tell when a song is playing, so it doesn't enforce
this: it is the caller's responsibility, and the SDK's documentation for those
calls says so. The firmware relies on the rule instead of protecting play.

**Direction (2026-10-20): firmware reaches users as published binaries.** Master
and panel firmware are built and attached to the repo's releases; the config
app checks for newer versions and offers to install them (master first). Users
never build firmware; power users who change the code can. To settle when it
is built: the release's contents and a manifest (versions, which master goes
with which panel firmware, config format version, checksums), and how the app
finds the latest release.

## Built (2026-10-20; the `monitor` example ran against master #2 the same evening)

The skeleton of the Rust SDK: crate **`krakenpad`** in `software/sdk`, in the
`software/` Cargo workspace. Its only dependency is `hidapi` 2.6.7 (the pinned
version), and it has no `unsafe` (`#![forbid(unsafe_code)]`).

**Build and test** from `software/`: `cargo test`, `cargo clippy --all-targets`,
`cargo fmt`. **It needs Rust 1.97 or later** (the pinned `rust-version`). First
built on 1.96 with `--ignore-rust-version`, then on **1.99.1** (the user updated
the same day): tests, clippy and fmt clean without the flag.

**The pieces** (`software/sdk/src/`):

| File | What |
|---|---|
| `protocol.rs` | Packet framing and reassembly, command and event codes. The firmware's and `tools/pad_cmd.py`'s framing, a third time |
| `config.rs` | `PadConfig` / `PanelConfig` / `ChannelConfig`, the validity rule, the `'W'` CRC |
| `messages.rs` | The master's answers, decoded: device info, pad status, telemetry samples, self-test, clock probe, LED frame |
| `transport.rs` | hidapi behind a small trait: finds the vendor interface by VID/PID, usage page `0xFF00`, usage 1 and the product string, and reads the OUT packet size from its report descriptor (1,024 at High Speed, 64 at Full Speed; 2026-10-21) |
| `pad.rs` | One pad: its reader thread, commands, state, the public `Pad` |
| `context.rs` | `Context`: the manager thread, the pads by player ID, the event queue |
| `events.rs` | `Event` and the queue |
| `fake.rs` | A fake master for the tests |

**How it behaves:**
- **Two HID handles per pad**, as `rustmaniax-sdk` does: the reader thread owns
  one and callers write through the other, so a write never waits on a read.
  On macOS that needs hidapi's `macos-shared-device` feature, which
  `rustmaniax-sdk` already turns on in deadsync's build.
- **Finding pads:** the manager thread scans every second. A new pad gets a
  handshake (device info, then the config and pad status, so its first
  snapshot is complete) and is registered under its player ID, and then
  `PadConnected` goes out. **Refused, with `PadRejected`**: a master whose
  protocol version differs from the SDK's (its player is then `None`: the
  rest of its device info isn't read), a duplicate player
  ID, or a **reserved player ID (4–7). A master straight from assembly reads 7**
  (all switches off). A refused master stays refused until it is replugged.
- **USB speed:** `DeviceInfo` carries the negotiated speed and both packet
  sizes, for the config app's connection status (protocol 2, 2026-10-21).
- **Stale input:** at open the reader discards whatever the master queued
  earlier (up to 200 ms), and transaction IDs start at an unpredictable value.
  A test confirms the discard matters: without it, a stale response made the
  pad register under the wrong player.
- **Commands:** one in flight per pad, matched by transaction ID; a late
  response is counted and dropped. Timeouts: 1 s; 3 s for set and reset config
  (they save first); 5 s for the self-test.
- **Events** that arrive during the handshake are held until the pad is
  registered, so every event names its pad and none comes before
  `PadConnected`. "Config was reset" comes from device info's flag at connect,
  not from the master's own event, which only the first program to open the
  pad would see.
- **Telemetry** is off at connect; `set_telemetry(true)` streams into the
  snapshot, which keeps the latest sample per panel.

**The API**: `Context::new`, `pads`, `pad(player)`, `poll_event`, `wait_event`,
`set_wake`; `Pad::snapshot`, `info`, `config`, `stats`, `device_info`,
`get_config`, `set_config`, `reset_config`, `pad_status`, `set_telemetry`,
`set_led_mode`, `self_test`, `zero_counters`, `send_led_frame`, `clock_probe`.
The doc comments carry the "not during gameplay" rule for saving.

**Simulated pads (feature `sim`, 2026-10-27):** `krakenpad::sim::Simulator`
puts the test fake master behind a real `Context` (`Simulator::context()`), so
an app runs the SDK's own code with no hardware. Its panels stream 50 Hz
samples: a fitted sensor rests at 88–114 raw with a little noise, an empty
socket at 13–18 (the measured ranges), and presses follow the master's config
with hysteresis. It can plug and unplug pads, silence panels and put weight on
sensors. Off by default, so games don't build it; the config app turns it on.
The fake master's termination check now follows DR's presence (the switch is
on DR, last in chain order).

**Tests:** 25 plus the doc example, 20 runs clean in a row. With `sim`, two
more (a held panel presses and releases by the config; a silenced panel and an
unplug reach the SDK as events): 31 in all. The fake master
shares the SDK's reading of the protocol, so it can't catch a misreading of the
firmware; the bench does that. **First run on the pad:** `cargo run --example
monitor` from `software/` prints events and live FSR values
(`docs/MASTER_BRINGUP.md` → "Config and the USB vendor interface").

**Choices made while building it, for review:**
- **How the SDK gets the firmware's shared values** (`docs/PANEL_CONFIG.md`
  left this open): for now it repeats them, and a test reads
  `firmware/shared/panel_config.h` and fails if any differ. That only works
  while both live in this repo; the longer-term answer is open ("Not decided"
  below).
- **Player IDs 4–7 are refused**, following "4–7 reserved". The alternative was
  accepting them as extra pads.
- **No logging dependency**: problems surface as `Pad::stats()` counters and
  `Error` values.

**Not built yet:** `krakenpad-c` (the C interface and its `cbindgen` header;
**not needed yet**, the user, 2026-10-20: it waits for a C or C++ program that
uses the SDK);
the row-by-row image helper for LED
frames; sample history for graphs (the snapshot keeps only the latest);
passthrough and panel updates; writing master firmware over HID; clock sync
(only the probe exists); underglow; reset animations; timestamped input.

## Config app (started 2026-10-27)

Crate **`krakenpad-config`** in `software/apps/config`, on `eframe` 0.37.0 (its
default wgpu renderer). Run it from `software/` with
`cargo run -p krakenpad-config`, or **`cargo run -p krakenpad-config -- --sim`
for simulated pads** (below).

**As built (2026-10-27):** a sidebar (Overview, Sensors, Lights, Updates; only
Overview has content) and the **Overview**: every connected pad as a card, side
by side. Each card has the master's firmware and build, its USB speed (at Full
Speed, the measured LED-frame cost) and packet sizes, a 3×3 grid of the panels
(lit while pressed, dimmed while not answering, outlined when a panel's config
doesn't match the master's, `T` on the terminated one; hovering one shows its
pad-status details), the termination check, and warnings for "config was
reset", unsaved console edits and bus passthrough. An event log sits below.
- **A thread of the app's own** (`worker.rs`) reads the SDK's event queue (C
  above) and runs every command that blocks: telemetry on at connect, device
  info and pad status every 2 s and soon after a presence or config event. The
  window only reads what it leaves, so it never waits on a master. While a pad
  is connected the window redraws at ~30 Hz for live telemetry, which has no
  event to wake it.
- **Simulated pads, `--sim`:** two pads from the SDK's simulator (feature
  `sim`, "Built" above): P1 as it should be, and P2 at Full Speed, with DR not
  answering, sensors only on U, L, C, R and D, and a master that lost its
  config. A **Simulator** window plugs and unplugs each pad's USB, presses a
  panel while the mouse holds it (at a chosen weight), and makes a panel stop
  or start answering (right-click).
- **Dev aid:** `KRAKENPAD_SCREENSHOT=out.bmp` saves the window ~2 s after
  start, then quits. eframe's own screenshot hook works only with its glow
  renderer.

**UI decisions** (the user, 2026-10-27):
- **DECIDED: a sidebar of sections** (e.g. Overview, Sensors, Lights,
  Updates), "for now".
- **DECIDED: every connected pad shown at once**, side by side, not a P1/P2
  switcher ("ideally, if possible").
- **The aim for thresholds:** a simple view and a power-user view without
  over-complicating either, and **arbitrary groups of sensors**, so adjusting
  the same set doesn't mean changing many values over and over. For
  comparison, the stock config app (open source) has four double sliders
  (up, center, cardinal, corner), one per panel in its advanced mode, plus
  inner-sensor, outer-sensor and one "custom" set on top, with fixed
  priorities between overlapping sliders; the custom set lives in the app's
  settings, not on the pad.
- **DECIDED: groups are saved selections, not owners of values.** Values stay
  per sensor, as the master stores them. A group is a named set of sensors;
  choosing it selects them, and one press/release double slider edits them
  all. Where they differ the slider shows the spread: dragging sets them all
  to one value, and nudge buttons move them all while keeping their
  differences. No priorities, nothing on the PC that can disagree with the
  pad, and groups may overlap. Rejected: groups that own values with a
  priority order (stock's model), because arbitrary groups would need a
  user-ordered priority, the UI would have to explain why a slider didn't
  move a sensor, and group values held on the PC could disagree with the
  pad's (after a console edit, or on another computer).
  - **Simple view:** a short list of built-in groups (All, Up, Center,
    Cardinals, Corners), each with its own slider. **Power view:** the 3×3
    grids, sensors selected by click and shift-click or by shortcut ("every
    S sensor", inner, outer), and any selection saved as a named group.
    Unpopulated sensors are skipped.
- **DECIDED: named groups live on the PC** (the app's settings), not on the
  pad. Tempting to keep them on the pad, but pads rarely move between
  computers; at worst a user sets them up again after changing PCs. **Config
  files** cover the rest: save a pad's config with its metadata (the named
  groups included) to a file and load it back, for backup, a computer change
  or sharing with friends (`docs/PANEL_CONFIG.md` → export and import).
- **DECIDED: an edit goes to one pad**, unless the user chooses to have both
  pads share the same config.
- **DECIDED: no manual save button.** A forgotten save button means settings
  lost at the next reboot.
- **DECIDED (the user, the same night): a change applies at once without
  saving, and the save follows about a second after the last change.** The
  pad responds while a slider is dragged, and a drag costs one save instead
  of one per second. **Needs the master:** two USB commands it doesn't have,
  "apply config" (update its copy and push it to the panels, no EEPROM
  write) and "save config", the same split as its console's `S`/`F` and
  `save`. Today every USB set config saves first. A new protocol version,
  since the SDK refuses a master on another. The pacing lives in the SDK
  (applies coalesced, then the delayed save), so deadsync gets it too; the
  results arrive as events. Unsaved changes are lost if the master loses
  power within that second. Not built.
- ~~**DECIDED: saves work the way stock's do.**~~ Superseded by the item
  above the same night, because the pad would only catch up once a second
  while a slider moves. Kept for the reasoning: every config
  write over USB is a save (as built), and the writes are rate-limited: a
  change goes out at once, then at most one write per second, and the latest
  change always goes out in the end, so nothing is dropped. That is the stock
  SDK's behaviour (`SetConfig` keeps the wanted config; its I/O thread sends
  it no more than once a second). No firmware change. The cost: while a
  slider is dragged, the pad catches up at most once a second (the lines on
  screen move at once); each write stalls the master ~1 ms, about 1 in 440
  up to ~35 ms (the no-saves-during-gameplay rule still holds: the app writes
  only when the user edits). It needs no firmware change, which is still the
  fallback.

**macOS trap, found starting it:** hidapi's device enumeration runs the calling
thread's run loop. eframe builds the app from inside winit's event handling on
the main thread, so `Context::new()` there re-entered winit, which panicked
("tried to handle event while another event is currently being handled"). The
app creates the `Context` in `main()` before the event loop starts. The SDK's
own enumeration after that runs on its manager thread, which is safe. Anything
else built on winit needs the same care, deadsync included ("Not decided"
below).

## Not decided

- **Whether `Context::new()` should start hidapi on the SDK's manager thread**
  (raised 2026-10-27, from the macOS trap above). Then it would be safe to call
  from any thread, including inside a winit handler, and no program would need
  to know about the trap. Today it starts hidapi on the caller's thread, so that
  a failure comes back from `new()` directly; the manager thread could hand the
  result back instead. deadsync creates its input backends on threads of its
  own, which would avoid the trap anyway.

- **How the SDK and the firmware share values once they live in separate
  repos** (raised 2026-10-20; the user is unsure). Today the SDK repeats the
  config format's values and a test reads the firmware's header. Options:
  1. **Keep that**, with the header copied into the SDK's repo and the test
     run against each new firmware release.
  2. **One source file** (say, a small TOML) that generates both the C header
     and the Rust constants, in a repo both use.
  3. **Share less.** The SDK can learn most of it from the master: the config
     version is in device info, the master applies defaults (Reset config) and
     validates (Set config names the first bad channel). What is left is the
     byte layout of each config version, which never changes once released.
     That needs shared **test vectors** (bytes and their CRCs) that every
     implementation checks, the same "shared test data" planned for a C++ port
     (above). Defaults, if the SDK ever needs them, could come from a new
     command.

- How the SDK takes over bus updates (`docs/FIELD_UPDATE.md`), which today go
  through the master's text-console passthrough. The master's side is built
  (USB bus passthrough, commands 0x0D/0x0E, 2026-10-20; `pad_cmd.py bus-h`
  works, with two early misses not reproduced since, `docs/BRINGUP_LOG.md`); what
  remains is porting `tools/panel_update.py`'s logic onto it.
