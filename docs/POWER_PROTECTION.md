# Power-input protection (reverse polarity + per-panel fuse) — DRAFT

> **STATUS: PROPOSED 2026-08-06 — NOT IMPLEMENTED.** No schematic or layout
> change has been made. This documents the reasoning, the measured as-built
> topology it has to fit into, and the implementation options, so the decision
> can be made deliberately. The open question is the trunk split (see
> "The trunk-split problem"), which is a layout-strategy choice.

## Threat model — what this actually protects against

**Reverse polarity.** Panel-to-panel 12V jumpers cannot be reversed — Micro-Fit
3.0 is keyed. The one credible reversal is the **PSU end of each column
harness: hand-crimped fork/spade lugs** onto the PSU studs. One swapped crimp
puts −12V on all three panels of that column. What dies: **C201** (aluminum
electrolytic, polarised — vents/bursts), the **AMS1117** on each brain, and
potentially 25× WS2815 per panel. That is the expensive, annoying failure this
guards against.

**Overcurrent.** A fault inside one panel (shorted LED, failed C201, solder
bridge, crushed wire) is today fed by the PSU's full output through the column
daisy-chain; PSU OCP is sized for the whole pad (3 columns), not one panel, so
a single-panel fault can pump many amps through one board before anything
trips. A per-panel fuse makes one panel's fault that panel's problem —
detectable and localised — instead of a column-level brownout/fire drill.

These decompose: the fuse **requires** separating local load from pass-through
(see below); reverse-polarity protection does not. They can be adopted
independently.

## Measured as-built topology (2026-08-06, from `dual-panel.kicad_pcb`)

The protection has to insert into this, so these facts drive the options:

- The 2mm 12V trunk runs **J205 (56.6, 52.6) → J208 (172.7, 49.6), F.Cu only**,
  and holds 2mm width end-to-end (verified by widest-path analysis).
- **The trunk is not a clean bypass — it doubles as the LED-field spine.**
  Five attachments sit along it:
  - **C201's positive pad** (66.1, 52.1) is directly on the trunk;
  - a **1.0mm tree tap** at (66.1, 56.1) beside C201;
  - a **0.3mm stub** at (72.3, 48.2);
  - **three 0.6mm-drill vias ON the trunk** at (75.3, 48.2), (106.3, 48.2),
    (139.8, 48.2) dropping to the B.Cu distribution that feeds the LED field
    (top row D203/D208/D213/D218 at y≈38.6 and onward).
- The 12V tree overall: ~623mm on F.Cu, ~325mm on B.Cu, 36× 0.6mm-drill vias.
- The last attachment is at x≈140; the trunk from there to J208 (x=172.7) is
  pure pass-through already.
- Brain 12V (AMS1117 input, R313/R314 sense divider) feeds off the carrier
  tree via J212 pin 1 / J213 pin 6 — so anything that protects the carrier's
  local node automatically protects the brain.

## Architecture: protect the local tap, not the trunk (for the fuse)

Anything in series with the IN→OUT pass-through would need 2.7A sizing instead
of ~1A, would stack its drop three-deep for the last panel, and — decisive for
the fuse — has **no selectivity**: a fault at panel 6 flows through panels 0
and 3's series elements too, so identical fuses on identical boards means the
wrong fuse can blow first. The fuse therefore belongs between the trunk and
the panel's own loads:

```
J205 ══ 2mm pass-through ══════════════════════ J208     (2.7A, pure copper)
              │ single tap
              F201  PTC resettable fuse          ← overcurrent
              Q201  P-FET, reverse-blocking      ← reverse polarity
              │  new net: +12V_LOCAL
              ├─ C201 470µF          (moves off the trunk, behind Q201)
              ├─ LED-field tree      (all 25 WS2815 feeds)
              └─ J212.1 → brain      (AMS1117, sense divider)
```

Local worst-case load: ~0.9A LED field (25 × 36mA full white) + ~60mA brain
≈ **1.0A**.

### Components

| Ref | Part | Spec | Sizing rationale |
|---|---|---|---|
| F201 | PTC resettable fuse, **1812** SMD | **hold ≥1.6A, trip ~3.2A, rated ≥16V (prefer 24V)** | 1.6× steady load covers PTC thermal derating (~−20% at 40°C ambient inside the pad). Reference series: Littelfuse 1812L, Bourns MF-MSMF; pick an in-stock LCSC equivalent — search "1812 PTC 1.6A 24V". ~0.1Ω → ~0.1W and ~0.1V drop at full white, invisible at 12V. Resettable over one-time: no spare-fuse stocking, trip self-clears on unplug |
| Q201 | P-channel MOSFET, SOT-23 | **Vds ≥ −30V, Id ≥ 3A cont., Rds(on) ≤ 80mΩ @ Vgs = −10V** | Must carry F201's trip current briefly, hence ≥3A. ≤80mW dissipation at 1A. **Check Vgs abs-max on the actual part** — see below |
| R2xx | 100k, 0603 | gate → GND | Gate pull-down; sets Vgs ≈ −12V in normal operation |
| D2xx | 10V zener, SOD-123 (**only if Q201's Vgs max is ±12V**) | gate–source clamp | The ubiquitous AO3401A is ±12V Vgs — a 12V rail sits exactly at abs-max, so it needs the zener (BZT52C10). A **±20V-Vgs part (DMP3099L-class) skips the zener entirely** — prefer that if LCSC stock allows |

**Q201 orientation: drain = supply side, source = load side, gate via 100k to
GND.** Normal polarity: the body diode precharges the load, then the channel
turns on (Vgs ≈ −12V) and drops millivolts. Reversed: Vgs never goes negative,
the FET stays off, and the local node **floats at 0V** — unlike a shunt/crowbar
diode, which leaves −0.5V across the electrolytic and LEDs while waiting for
the fuse. No fault current, nothing to reset, panel is simply dark.

Inrush: C201's charge at plug-in flows through the PTC for well under a
millisecond — far below a thermal PTC's reaction. Not a nuisance-trip risk.

### Refdes / net proposals

New net `+12V_LOCAL` (carrier). New parts take carrier 2xx numbers: F201,
Q201, next-free R2xx/D2xx. All 9 panels populate them — the identical-panels
invariant holds.

## The trunk-split problem — the open decision

Because the trunk doubles as the field spine, "insert protection at the tap"
means restructuring those five attachment points. Three strategies:

### Option A — role swap: new bypass, old trunk becomes the protected spine

Route a **new, clean 2mm J205→J208 bypass** and cut the old trunk free of both
connector pads. The old trunk — with **all five existing attachments and C201
untouched** — becomes `+12V_LOCAL`, fed through F201→Q201 from the new bypass
at its left end.

- Least surgery to the existing tree: zero re-homing of taps or vias.
- The new bypass is one fat route. On B.Cu it's a straight shot with **3–4×
  0.6mm-drill vias at each end** (~1.5–2A conservative per via, so 3+ for
  2.7A; these join the existing 44-via POFV question for JLC, changing
  nothing about it). On F.Cu it needs a clear 2mm corridor, which the LED
  field probably doesn't have.
- **Check before picking a B.Cu corridor: the FSR runs are on B.Cu**
  (accepted-rev-A risk). The bypass carries downstream panels' LED PWM
  transients; keep it away from the FSR routes (map them first) and rely on
  the untouched In1/In2 GND planes for F.Cu↔B.Cu isolation.
- Cut points: old trunk detaches from J205.1 and from J208.1 (the x≈140–172.7
  stretch was pure pass-through and can be deleted or left on the bypass
  side).

### Option B — keep the trunk as bypass, build a new protected spine

The trunk stays connected J205→J208 exactly as now; each of the five
attachments is re-homed onto a **new ~1mm `+12V_LOCAL` spine** running from
the protection output: C201's pad, the 1.0mm tap, the 0.3mm stub, and the
three on-trunk vias (delete each, re-drop an adjacent via from the new spine
into the same B.Cu tree segment).

- More individual edits than A (five re-homes + a new spine corridor with the
  same routing-space question), but the pass-through keeps its current,
  already-verified geometry.

### Option C — series FET at the panel entry, no split at all

If the split is deferred: reverse-polarity protection alone can sit **in the
trunk immediately at J205**, before the first tap. Everything — trunk, tree,
brain, J208 onward — is behind it, and no restructuring happens.

- FET sizes up: 2.7A continuous → **SOIC-8 / PowerPAK class, ≤20mΩ**
  (SI4435-class), still cheap. Three in series per column drop ~0.16V total at
  full load — negligible.
- Covers the *actual* credible reversal (PSU lugs) completely, since
  downstream panels receive already-protected power through keyed connectors.
- **Cost 1: no per-panel fuse** — this option alone doesn't provide one, and a
  later fuse still needs the A/B split.
- **Cost 2: the passthrough variant breaks unless handled.** A
  connectors-only carrier must still pass 12V through, so the variant would
  have to populate Q-only, or the footprint needs a designed-in bridge
  (0Ω-strap pads across drain–source on the DNP list). A/B don't have this
  problem — their protection parts simply join the DNP list.
- Failure surface: a failed-open FET in panel 0 darkens the column (any
  series element shares this; PTCs in A/B fail the same way for one panel).

### Complementary, board-free: harness fuses

Three **inline blade-fuse holders (5A) at the PSU end of each column** cover
the one thing per-panel protection can't — a shorted inter-panel cable or
trunk — with zero board changes. Selectivity against the panel PTCs is
imperfect in a hard short (both are slow devices), fine in overload. This is
a harness decision and can be adopted regardless of A/B/C.

## Interactions and side effects

- **`SENSE_12V` gets more honest, not less** (options A/B): R313 feeds from
  the brain rail, which arrives via J212 from `+12V_LOCAL` — so the sense line
  reports "protected local rail is up", exactly its documented job of gating
  whether it's safe to drive the WS2815s. A tripped PTC or off FET reads as
  LEDs-stay-off, correctly. (Option C: sense sits behind the entry FET, same
  effect.)
- **Test points:** keep one 12V TP on the raw bus side and one on
  `+12V_LOCAL` — being able to probe both sides of the protection is exactly
  what you want at bring-up.
- **Placement:** the natural patch is the existing tap neighborhood **beside
  C201 / north of the trunk near J205** — same region that already fits a
  10mm electrolytic. All new parts are SMD and low-profile; no cavity/fit-test
  impact, no outline change.
- **Silent failure modes, accepted:** a FET failed short loses reverse
  protection undetectably (no cheap detection exists; the exposure window is
  the next re-crimp of the PSU harness). A PTC that has tripped many times
  ages toward higher resistance — at 0.1V/0.1W margins this stays irrelevant
  for a long time.
- **What this does NOT change:** the trunk's verified 2mm/2.7A geometry (A
  keeps it via a new route, B/C keep it literally), RS-485/INT/FSR circuits,
  the brain, and the identical-panels invariant.

## Decision checklist (in order)

1. **Pick the architecture:** A (role swap), B (new spine), C (entry FET,
   defer the fuse), or harness-only. A vs B is a layout-taste call; C changes
   what protection you get.
2. If A/B: **map the FSR B.Cu runs** and pick the bypass/spine corridor clear
   of them.
3. **Pick parts on LCSC**: PTC (verify voltage rating ≥16V and hold current
   at 40°C), P-FET (verify Vgs abs-max — decides whether the zener is
   needed), confirm footprints.
4. Schematic edit (new net + 2–4 parts), layout edit per chosen option,
   refill zones, **re-run the three-check discipline** (DRC + parity +
   netlist diff), and re-run the widest-path check on both the new
   pass-through and `+12V_LOCAL`.
5. Decide the harness blade-fuse question separately.
