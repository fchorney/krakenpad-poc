# Modular Panel Count (Passthrough PCBs)

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
- **INT**: still has the same single-conductor termination as a real panel (screw
  terminal, per the 2026-07-10 INT rework — no Micro-Fit, no GND conductor; the master
  end is a 9–10 position pluggable terminal block), for mechanical/cable uniformity —
  but the terminal is **not connected to anything** on the passthrough board (no local
  pull-down, no active drive). This relies on the existing master-side pull-up and the
  already-documented safe failure mode ("disconnected/floating wire reads HIGH = not
  pressed").
- **RS-485 termination**: 120Ω termination resistor footprint, **populated only if this board
  ends up in the last physical position of the RS-485 chain**. Because either a real panel or
  a passthrough board could end up in that last slot depending on kit configuration, this
  switchable/populatable termination footprint must exist on **both** real panel PCBs and
  passthrough PCBs — it can no longer be hardwired to "panel 8" specifically.

## Termination Jumper Mechanism (resolved)

Both real panel PCBs and passthrough PCBs get a **3-pin (SPDT-style) jumper header** tapped
off the RS-485 A/B differential pair, with the 120Ω termination resistor always populated on
every board:

- **Common pin**: tied to the RS-485 A/B tap.
- **Position 1 (default, shipped state on every board)**: plain PCB trace — not terminated.
  No component populated here; a trace and a 0Ω resistor are electrically identical, so a
  resistor would just be an extra assembly step for no benefit.
- **Position 2**: routes through the always-populated 120Ω resistor — terminated.

The jumper cap is always installed on the header, just moved between the two positions rather
than added/removed loose — this avoids customers losing a physical jumper cap when
reconfiguring. A customer only needs to move the jumper to position 2 on whichever single
physical board (real panel or passthrough) ends up last in their kit's chain; every other
board stays at the default position 1.

### Physical control: DPDT slide switch (replaces jumper header)

Preferred over a loose jumper cap for end-user-facing hardware: a **DPDT slide switch** does
the same job with no removable part to lose.

- **Pole 1**: routes the 120Ω termination resistor in/out of the RS-485 A/B pair (same
  function as the jumper header above).
- **Pole 2**: ganged to the same physical actuator, wired to a spare panel MCU GPIO (internal
  pull-up; switch pulls it LOW in the "terminated" position). Guarantees the reported state
  always matches the real electrical state, since it's the same switch.

### Software detection of misconfigured termination

Termination is purely a signal-integrity (impedance matching) measure — it prevents reflections
at the end of the bus. At this project's cable lengths (<3m) and 1 Mbps baud, a missing or
misplaced termination resistor is unlikely to actually corrupt data (see
`CLAUDE.md` RS-485 baud rate notes), so **monitoring for bus errors is not a reliable way to
detect a termination mistake** — there may be no observable data errors even when it's wired
wrong. Detection instead reads the switch's physical state directly, independent of whether it
currently affects data integrity:

- Each panel reports its pole-2 GPIO state as one extra field in its existing RS-485 telemetry
  poll response.
- The master already knows, from panel DIP-switch identities, which panels are present and
  therefore which single panel *should* be last in the chain for the current kit configuration.
- Master cross-checks expected-last-panel against actual reported termination state and flags:
  - **None terminated** — "no termination detected, check the switch on panel `<expected>`"
  - **Wrong panel terminated** — "termination is on panel `<actual>`, should be on panel `<expected>`"
  - **Multiple terminated** — "multiple panels terminated, should be exactly one"

This is a deterministic hardware-state check, not a heuristic — no error-rate thresholds or
false-positive risk from unrelated bus/cable issues.

## Detecting a misconfigured panel-ID DIP switch (2026-07-09)

Stock SMX has to multiplex the DIP-switch ID into the shared interrupt-line sensor data,
because that line is genuinely shared across panels — the only way to know whose data
you're looking at is if the panel tells you. **This project doesn't have that ambiguity**:
each panel's INT wire is a home-run, one wire per physical slot, so the master already
knows *which physical position* asserted a press with zero reliance on any self-reported
ID. The DIP ID only matters for RS-485 (LED targeting, telemetry labeling) — so a
wrong-but-unique DIP setting breaks lighting/telemetry attribution, not press-detection
latency or bus function (duplicate IDs are a separate, already-handled case — see the
2026-07-11 multi-panel bring-up notes above).

**Detection idea (not yet implemented):** cross-correlate the two independent identity
channels the master already has. If INT-slot 3 asserts a press but the RS-485 telemetry
for panel ID "3" shows no threshold crossing at that same moment (while some other ID, say
"5", does), that's a deterministic signature of a misconfigured DIP switch on that physical
board — no new hardware, just a correlation check in master firmware. Cleanest as a guided
setup step (have the installer press panels 0→8 in physical order once) rather than passive
run-time monitoring, similar in spirit to the termination cross-check above.
