# Modular Panel Count (Passthrough PCBs)

> **Status: forward-looking design.** No passthrough board has been built. The
> rev-1 carrier can serve as one with only its connectors, `SW202` and `R201`
> fitted (`docs/DUAL_PANEL.md` → "Passthrough variant"). This captures the
> decision so a future kit build doesn't re-derive it. The connector and INT
> details were re-checked against the as-built boards on 2026-08-04.

This document captures the design decision for supporting kits with fewer than 9 populated
panels (e.g. a 4-panel/single-play customer), without requiring custom wiring harnesses per
panel count.

## Motivation

Not every customer plays 8-panel/doubles. A single-play (4-panel) customer only needs panels
1 (U), 3 (L), 5 (R), 7 (D) — the diagonals (0, 2, 6, 8) and center (4) go unused. Selling a
full 9-panel kit to these customers means charging them for RP2040s, LEDs, and FSRs on 5
panels they'll never use, and building/stocking a separate custom cable harness per supported
panel count adds SKU complexity.

## Decision: Passive Passthrough PCBs

Unpopulated panel slots get a **passive, connector-only PCB** instead of a full panel PCBA.
This board has no RP2040, no LEDs, no FSRs, no DIP switch — just connectors and pass-through
copper. This means:

- **One universal 9-slot cabling/daisy-chain design** regardless of how many real panels a
  kit includes. No per-panel-count harness variants.
- Since panel identity is already resolved via the 4-position DIP switch + software (not
  physical position), any real panel PCB can be placed in any of the 9 physical slots — the
  passthrough boards simply occupy the slots the customer didn't buy a real panel for.
- **Upgrade path is free**: a customer can start with 4 real panels + 5 passthroughs, then
  later buy more real panels and swap them in one at a time with zero rewiring.
- Customers only pay for the BOM cost of panels they actually use.

## Passthrough PCB Requirements

- **Power**: 2-pin Micro-Fit IN + 2-pin Micro-Fit OUT, straight pass-through trace.
- **RS-485**: 3-pin Micro-Fit IN + 3-pin Micro-Fit OUT, straight pass-through trace.
- (Corrected 2026-07-10: no position-specific IN-only builds — all boards, real and
  passthrough, carry the full IN+OUT set so any board works in any slot; an OUT at the end
  of a chain simply sits empty.)
- **INT**: same termination as a real panel — a 2-position 5.08mm screw terminal
  (KANGNEX WJ500V-5.08-2P) carrying **signal on pin 1 and a dedicated GND return on
  pin 2**, cabled as a twisted pair, home-run to one of the master's nine JST XH
  headers (J3–J11). Kept for mechanical/cable uniformity, but the terminal is **not
  connected to anything** on the passthrough board (no local pull-down, no active
  drive). This relies on the existing master-side pull-up and the already-documented
  safe failure mode ("disconnected/floating wire reads HIGH = not pressed").
- **RS-485 termination**: 120Ω termination resistor footprint, **populated only if this board
  ends up in the last physical position of the RS-485 chain**. Because either a real panel or
  a passthrough board could end up in that last slot depending on kit configuration, this
  switchable/populatable termination footprint must exist on **both** real panel PCBs and
  passthrough PCBs — it can no longer be hardwired to "panel 8" specifically.

## Termination: the DPDT switch on every board (built)

Both real panels and passthrough boards need switchable termination, because
either could end up last in the chain. Rev 1's carrier has it: **`SW202`**, a
DPDT slide switch. Pole A puts `R201` (120 Ω, always fitted) across the pair;
pole B reports the position on `TERM_SENSE` (GPIO, LOW = terminated), so the
reported state comes from the same actuator as the electrical one. A 3-pin
jumper header was the first idea; the switch replaced it so there is no loose
cap to lose. Wiring: `docs/DUAL_PANEL.md` → "Signal conventions".

**Detection is built.** Each panel reports `TERM_SENSE` in its `'h'` status
reply, and after every `H` sweep the master checks that exactly one panel is
terminated: the last one, in the documented chain order, that answered. It
prints a warning, never an error, because the bus ran with zero errors
unterminated, mid-chain and doubled (`docs/MASTER_BRINGUP.md` → "Termination
check", which also lists what the check cannot see). A passthrough board has no
MCU, so it couldn't report its switch; a kit with a passthrough at the end of
the chain would need the check to allow for that.

## Detecting a misconfigured panel-ID DIP switch (built)

Each panel's INT wire is a home run, so the master knows which physical slot
asserted a press without relying on any self-reported ID. The DIP ID only
matters for RS-485 (LED targeting, telemetry labels), so a wrong-but-unique DIP
setting breaks lighting and telemetry attribution, not press detection.

The **`'I'` slot ↔ panel-ID self-test** (`docs/RS485_PROTOCOL.md`) is the
check: the master asks each ID to pulse its INT line and records which slot
the pulse arrives on, so it learns the real mapping instead of assuming it. It
also catches duplicate IDs (two slots fire). The 2026-07-09 idea of
correlating presses with telemetry during a guided setup step is not needed.
