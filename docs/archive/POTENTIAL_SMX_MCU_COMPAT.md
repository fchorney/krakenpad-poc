> **ARCHIVED 2026-07-23** — Not a planned feature — reference only, in case SMX-MCU compatibility is ever revisited.

# Potential: SMX MCU Compatibility Mode

This document captures a design discussion about what it would take to make the custom panel PCBs work with the **original SMX MCU** instead of the custom master MCU. This is **not a planned feature** — it is recorded here for reference if the idea is revisited later.

## Motivation

The custom panel PCBs are designed to work with the custom master MCU (Teensy 4.x). However, the hardware could theoretically be adapted to support the original SMX MCU as a drop-in panel replacement, allowing the custom panels to be used with stock SMX software and leaderboard eligibility (though that is explicitly out of scope for this project's goals).

## SMX MCU Data Bus

The original SMX data bus is:
- **UART, 250000 baud, 8N1**, single-ended
- **Strictly unidirectional**: MCU → panels only (TX + GND). No RX line on the bus at all.
- **Physical connector**: RJ-12, but only two conductors are active (TX and GND).
- No power is carried on the data cable.

Sensor data never travels back over the data bus — all panel-to-MCU feedback is handled entirely by the separate signal wire (see below).

## SMX Signal Wire (Sensor Readback)

The original SMX uses a dedicated per-panel signal wire for sensor readback, not UART RX. The mechanism is a **clocked parallel readout**:

- MCU sends an 81-byte clock burst over the data bus.
- Each panel responds bit-by-bit on its individual signal wire, encoding FSR sensor state as an 80-bit value, synchronized to the incoming clock bytes.
- This is a timing-critical, side-band protocol — effectively a shift-register clocked by the bus traffic.
- Signal wire operates at **5V logic** (SMX MCU is 5V).

Full details of the protocol are in `../../stepmaniax-sdk-mp` (branch `fc/data-bus-deep-dive`) → `docs/INTERNAL_BUS_PROTOCOL.md`.

## What Would Need to Change

### Hardware

#### Panel 0 (first in chain) — RJ-12 entry point
The SMX MCU outputs via RJ-12. Panel 0 (or an adapter board between the SMX MCU and panel 0) would need an RJ-12 jack to receive the TX + GND signal. Adding an RJ-12 to the panel PCB footprint is awkward given the 5×5" size constraint and existing Micro-Fit connector layout. A small **adapter/breakout board** between the SMX MCU and panel 0 is the cleaner path.

#### Inter-panel data bus (panels 1–8) — reuse existing differential pairs
The existing Micro-Fit 3-pin RS-485 connectors and differential pair wiring can be reused. The THVD1429 transceivers on each panel already convert between single-ended UART and differential A/B — they are agnostic to the protocol running over them. In SMX compat mode, the transceivers would simply always be in **receive mode** (never transmit), acting as a differential line receiver for the single-ended SMX UART signal injected at panel 0. No hardware change needed for panels 1–8 on the bus side.

#### Signal wire — level shifting required
Each panel would need a **level shifter** on the signal wire GPIO (RP2040 is 3.3V; SMX MCU expects 5V). A BSS138-based bidirectional level shifter per panel is sufficient. The signal wire is a home-run back to the SMX MCU (same topology as the custom INT wire), so the connector could mirror the existing INT termination style (single-conductor screw terminal — the INT connector was reworked from Micro-Fit 2026-07-10).

#### Power — no change
Power continues to come from the custom 12V PSU daisy-chain. The SMX MCU carries no power on its data cable, so power delivery is unchanged.

### Firmware

#### Data bus parsing
Core 1 would need a second protocol mode that:
- Speaks 250kbaud 8N1 instead of 1 Mbps
- Parses SMX commands: `'R'` (reset), `'w'` (config write), `'G'` (poll loop start), `'4'`/`'2'`/`'3'` (LED data)
- Handles BREAK-terminated framing (RP2040 UART peripheral can detect BREAK, or use PIO)
- Maps incoming SMX LED data (inner 3×3 + outer top/bottom split) onto the 25 WS2815 LEDs

#### Signal wire clocked readout
This is the most complex firmware piece. A PIO program would need to:
- Watch for the 81-byte clock burst on the RX line
- Shift out the 80-bit FSR sensor response on the signal wire GPIO, bit-by-bit, synchronized to incoming clock bytes
- Encode FSR ADC readings into whatever bit format the SMX MCU expects (exact encoding would need to be confirmed from the protocol docs)

The RP2040 PIO is well-suited for this kind of cycle-accurate side-band protocol, but it requires careful implementation and testing against actual SMX MCU hardware.

#### Mode selection
A clean implementation would use a compile-time flag (e.g. `#define SMX_COMPAT_MODE`) that swaps the entire comms stack, keeping the custom protocol path clean and unaffected.

## Summary

| Area | Effort | Notes |
|------|--------|-------|
| Inter-panel bus wiring | None | Existing differential pairs reusable as-is |
| Panel 0 RJ-12 entry | Low | Adapter board preferred over adding RJ-12 to panel PCB |
| Signal wire level shifting | Low | One BSS138 shifter per panel |
| Data bus firmware | Medium | BREAK framing + LED command parsing |
| Signal wire PIO firmware | High | Cycle-accurate clocked readout; most complex piece |

Overall: achievable in ~2–3 weeks of focused firmware work, with minor hardware additions. The RP2040 is capable of it, but the signal wire PIO implementation requires thorough reverse-engineering of the exact bit encoding from the protocol docs.