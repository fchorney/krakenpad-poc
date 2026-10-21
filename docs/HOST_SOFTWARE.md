# Host software — SDK and apps

**Status: the SDK's Rust skeleton is built (2026-10-20) and tested against a
fake master; it has not talked to a real pad yet.** The C interface and the
apps are not built. What runs on the PC: the SDK that games link, and the apps
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
| `egui` / `eframe` | latest release when the apps are started, then pinned exactly | apps only; not in the SDK or deadsync |
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

**Tests:** 25 plus the doc example, 20 runs clean in a row. The fake master
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

## Not decided

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
