# Timestamped input events — FUTURE IDEA (firmware/software)

> **STATUS: IDEA, jotted 2026-08-06.** Not designed, not scheduled. Zero
> hardware impact — this is entirely firmware (master) + host-side protocol.
> Captured so it isn't lost; design it when the USB protocol work starts.
> Related: `docs/USB_PROTOCOL.md`, the USB polling-rate notes in `CLAUDE.md`.

## The idea (as proposed)

Instead of chasing 8kHz USB polling to minimise timestamp error on step
events: **the master timestamps each input edge locally with high precision,
sends the event with its timestamp (or timestamp delta) in the HID report,
and a sync/jitter process keeps the device clock and host clock related.**
The host then knows *when the step actually happened* — with microsecond-class
accuracy — regardless of when the USB report arrived.

## Why this works

Polling rate then only affects **latency** (how soon the host learns about
the event), not **timing accuracy** (when the event occurred). Judgement/
scoring in a rhythm game cares about the second number far more than the
first. Today the event time is inferred from USB arrival time, so every
source of transport jitter — polling phase (±500µs at 1kHz, ±62.5µs at 8kHz),
OS scheduling, driver batching — lands directly in the scoring timestamp.
With device-side timestamps, all of that drops out; even 1kHz polling would
carry perfect timing information, and the achieved-host-side-polling-
consistency question (the current open USB question) stops mattering for
accuracy.

The capture point is already ideal: the 9 INT lines are hardware interrupts
on the Teensy — the ISR can grab a timestamp within ~100ns of the edge
(ARM DWT cycle counter at 600MHz, or `micros()`). The interrupt wire exists
precisely so press detection is decoupled from every bus cycle; this extends
that decoupling all the way into the host's timeline.

## Sketch of the pieces (to be designed properly later)

- **Device timebase:** free-running counter on the Teensy (DWT cycle counter
  or a µs timer). INT ISR latches it per press/release edge.
- **Report format:** HID input report carries the panel/edge event plus the
  device timestamp (absolute µs, or delta from report send time). Multiple
  events per report need per-event timestamps — that also fixes the "two
  steps inside one polling interval" ambiguity that no polling rate fixes.
- **Clock sync:** NTP-style exchange over HID feature reports or the control
  channel: host sends t_host, device answers with t_device (+ its RX/TX
  local times); host estimates offset + drift (crystal drift is tens of ppm,
  so ~ms-per-minute — a slow PLL / linear regression over periodic probes is
  plenty). USB SOF frames are another possible shared tick (device sees SOF
  at 125µs intervals on HS; host can relate SOF to its clock).
- **Host side:** a custom input path in the target game (ITGmania/DeadSync)
  that consumes (event, device_time) and maps to the game clock via the sync
  estimate — instead of stamping events with arrival time. The protocol is
  custom and open anyway; this is exactly the kind of thing it exists for.

## Open questions for when this is picked up

- How the game engines' input pipelines accept externally-timestamped events
  (ITGmania input arch; DeadSync is presumably more malleable).
- Sync transport choice (feature-report ping vs SOF-based) and required probe
  cadence for the drift budget.
- Whether the RS-485 FSR telemetry timestamps should ride the same timebase
  (probably yes — one master timebase, everything referenced to it).
- Interaction with the 8kHz stretch goal: likely reframes it — 8kHz becomes a
  latency optimisation only, and may not be worth host-side jitter fights.
