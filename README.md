# KrakenPad

Open-source dance pad controller hardware and firmware, built for modern ITG-style
rhythm games (Stepmania, ITGmania, DeadSync, etc.).

Commercial dance pad electronics haven't kept pace with what competitive/high-level
play actually wants: low, consistent input latency, high-resolution force sensing,
and hardware/firmware you can actually inspect, modify, and fix yourself. This
project is a ground-up replacement controller — sensor boards, main controller, and
firmware — designed around those priorities instead of around vendor lock-in or
proprietary protocols.

It is not tied to any one game or platform — it just speaks USB HID, so anything
that reads a standard controller/joystick works.

## Priorities

- **Low, consistent input latency.** Presses are detected and reported by dedicated
  interrupt-driven hardware, not discovered via a slow polling cycle — eliminating
  the multi-millisecond blind spots that periodic-scan designs are prone to.
- **High-resolution force sensing.** Each panel reads continuous per-sensor
  pressure (not just a binary switch), sampled at kHz-class rates, enabling more
  accurate and tunable press/release detection than simple threshold switches.
- **Higher, more consistent USB polling and LED refresh rates** than older
  commercial hardware typically offers, for tighter timing and smoother panel
  lighting.
- **Fully open hardware and firmware.** Schematics, PCB layouts, and firmware
  source are all open — no proprietary authentication, no closed protocol, no
  black-box firmware blobs. Anyone can build, modify, or repair their own unit.
- **Standard, open software compatibility.** Presents as a generic USB controller
  so it works out of the box with the open-source rhythm game ecosystem, with no
  vendor-specific driver or SDK required.

## Hardware Scope

The project is initially targeted at retrofitting existing SMX-style dance pad
frames — reusing the frame and panel mechanics while replacing the electronics
(control unit and panel sensor/LED boards) entirely. The design isn't
SMX-compatible at the protocol or hardware level; it simply fits the same
frame as a drop-in electronics replacement, since that's the hardware most
readily available for this kind of upgrade.

## Status

Active development — architecture and core electrical decisions are settled,
prototype hardware has been bench-validated end-to-end, and PCB layout is in
progress. See `CLAUDE.md` and `docs/` for detailed design notes and specs.

## AI Disclosure

This project was built with AI assistance (Claude), used mainly for the grunt
work: research and part-comparison legwork, documentation drafts, firmware
prototyping, design-review passes, and scripted KiCad edits/fab outputs. All
design decisions, schematic wiring, PCB layout, and physical testing were done
by a human.
