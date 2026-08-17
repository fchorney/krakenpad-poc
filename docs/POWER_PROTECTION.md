# Power-input protection (reverse polarity + per-panel fuse) — REJECTED FOR REV A

> # ⛔ DECIDED 2026-08-17: NOTHING GOES ON THE BOARD.
>
> **Options A, B and C are all rejected for rev A. The board ships unprotected
> and that is deliberate, not an oversight.** Do not re-propose this without
> new information — the reasoning is recorded below and the inputs that drove
> it have already been corrected once.
>
> **What protection actually exists:** the trunk **T8A** fuse, keyed XT30 and
> Micro-Fit connectors, engraved `+12V`/`GND` on the printed fan-out carrier,
> and **metering the fan-out before first power-on**.
>
> **Accepted risk, stated plainly:** a mis-wire at the Wago fan-out reverses
> **all nine panels and the underglow at once** — C201 (polarised, vents), nine
> AMS1117s, and up to 225 WS2815s. It is a single connection, checked once with
> a multimeter, and it is the price of shipping rev A unprotected.
>
> **Per-column harness fuses are DEFERRED, not rejected** — see below. That
> deferral is cheap; this one is not.

## ⚠ The two deferrals are not equivalent

| | reversible later? |
|---|---|
| **Board protection (A/B/C)** | ❌ **No.** Without the footprints, adding it later is a board respin. This decision closes with the fab order. |
| **Per-column harness fuses** | ✅ **Yes, freely.** Inline holders at the fan-out, no board involvement. Can be added the day a fault makes them look worthwhile. |

If this is ever revisited *while already in the layout for another reason*, the
cheap middle path is to **add the footprints as DNP** — F201/Q201 pads cost
nothing at fab, and Option C's series FET only needs 0Ω strap pads across
drain–source so an unpopulated board still passes current. That preserves the
option for the price of layout time alone.

## Why it was rejected

- **The reversal is a one-time assembly error, not repeated exposure.** See the
  corrected threat model below — the original rationale described hardware that
  no longer exists.
- **Stock SMX has no fuses either.** Its only current ceiling was the Daygreen
  converter current-limiting at 75 W. Per-column fusing would put this build
  *ahead* of stock, which is user-proofing a product rather than protecting a
  prototype.
- **This is a prototype with 2 spare panels**, built and wired by the person who
  designed it, not a kit shipped to strangers.

## Threat model — CORRECTED 2026-08-17

> ⚠ **The original threat model described deleted hardware.** It read: *"The one
> credible reversal is the PSU end of each column harness: hand-crimped
> fork/spade lugs onto the PSU studs. One swapped crimp puts −12V on all three
> panels of that column."* **There are no lugs and no PSU studs** — the
> 2026-08-08 teardown found the supply is a brick with one captive output. That
> sentence drove the whole document's urgency and was false by the time the
> decision was made.

**Reverse polarity.** The actual 12V path is
`PSU captive cable → XT30 (keyed) → T8A fuse → Wago fan-out → Micro-Fit (keyed) → columns`.
Both connectors are keyed and cannot be mated backwards, so the credible
reversals are all **build-time wiring errors**:

1. **Mis-soldering an XT30 half** — we fit all three, so all three are exposed.
2. **Mis-wiring at the Wago fan-out** — a +12V lead into the GND block. The two
   rails are identical orange blocks, which is exactly why the printed carrier
   engraves `+12V`/`GND`.
3. **Mis-reading the PSU's captive-cable polarity when cutting the barrel** —
   both conductors are black. Already covered by the "meter before cutting"
   warning in `hardware/harness/12v-trunk.yml`.

**All three are caught by a multimeter before first power-on, and none recur.**
The original model assumed *repeated* exposure — re-crimping lugs during
service. The redesigned harness is build-once, then keyed and lever-locked.

**What got worse, and it is the one real argument for protection:** a fan-out
mis-wire reverses **the entire pad at once**, where the old lug model reversed
one column. Bigger blast radius, but a single point to check rather than three.

What dies in that case: **C201** (aluminum electrolytic, polarised —
vents/bursts), the **AMS1117** on each brain, and potentially 25× WS2815 per
panel.

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

Anything in series with the IN→OUT pass-through would need 1.3A sizing instead
of ~0.44A, would stack its drop three-deep for the last panel, and — decisive for
the fuse — has **no selectivity**: a fault at panel 6 flows through panels 0
and 3's series elements too, so identical fuses on identical boards means the
wrong fuse can blow first. The fuse therefore belongs between the trunk and
the panel's own loads:

```
J205 ══ 2mm pass-through ══════════════════════ J208     (1.3A, pure copper)
              │ single tap
              F201  PTC resettable fuse          ← overcurrent
              Q201  P-FET, reverse-blocking      ← reverse polarity
              │  new net: +12V_LOCAL
              ├─ C201 470µF          (moves off the trunk, behind Q201)
              ├─ LED-field tree      (all 25 WS2815 feeds)
              └─ J212.1 → brain      (AMS1117, sense divider)
```

Local worst-case load: ~0.375A LED field (25 × 15mA full white) + ~60mA brain
≈ **0.44A**, or **0.56A** sized at 20mA/pixel for binning headroom.

**Corrected 2026-08-16, down from 1.24A.** The datasheet's 15mA is **per pixel,
not per RGB channel** — WS2815 wires its three dies in series and shorts out the
unlit ones to hold pixel current constant. The old 47mA/LED (and the 36mA before
it) came from multiplying 15mA by three. Bench-measured at 10.5mA/pixel;
`hardware/harness/README.md` → "Power budget" carries the readings and sources.

**Note for this circuit specifically: pure red draws the same as full white.**
The protection worst case is *any* saturated frame, and no firmware colour
policy can lower it.

### Components

| Ref | Part | Spec | Sizing rationale |
|---|---|---|---|
| F201 | PTC resettable fuse, **1812** SMD | **hold ≥1.0A, trip ~2A, rated ≥16V (prefer 24V)** | 1.6× the 0.56A headroom load covers PTC thermal derating (~−20% at 40°C ambient inside the pad). Reference series: Littelfuse 1812L, Bourns MF-MSMF; pick an in-stock LCSC equivalent — search "1812 PTC 1A 24V". Resettable over one-time: no spare-fuse stocking, trip self-clears on unplug. **Lowered from 2.0A hold 2026-08-16** when the per-pixel/per-channel error was found |
| Q201 | P-channel MOSFET, SOT-23 | **Vds ≥ −30V, Id ≥ 2A cont., Rds(on) ≤ 80mΩ @ Vgs = −10V** | Must carry F201's trip current briefly, hence ≥2A (**lowered from 4A 2026-08-16**). ~25mW dissipation at 0.44A. **Check Vgs abs-max on the actual part** — see below. SOT-23 is now comfortably inside its package rating; the earlier note about needing SOT-223 or a PowerPAK no longer applies |
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
- The new bypass is one fat route. On B.Cu it's a straight shot with **4–5×
  0.6mm-drill vias at each end** (~1.5–2A conservative per via; 1.3A now needs
  only one, so the 4–5 is pure margin — these join the existing 44-via POFV
  question for JLC, changing nothing about it). On F.Cu it needs a clear 2mm corridor, which the LED
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

- FET sizes up: 1.3A continuous → SOT-23 still suffices here, though
  **SOIC-8 / PowerPAK class, ≤20mΩ** (SI4435-class) is cheap insurance. Three in
  series per column drop well under 0.1V total at full load — negligible.
- Covers the *actual* credible reversal (miswiring at the 12V fan-out) completely, since
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

### Complementary, board-free: per-column harness fuses — DEFERRED 2026-08-17

**Not rejected, deferred** — and freely reversible, since it touches no board.

⚠ **The 5A figure this section originally carried is void.** It was sized on the
pre-correction ~2.7A/column. **A column is 1.3A**, so the right part is a
**T2A slow-blow** (plus **T3A** for the underglow's 2.44A). Slow-blow is
mandatory, not preference: three panels × 470µF = **1410µF** of inrush would
nuisance-trip a fast 2A.

That correction matters more than it looks, because it is what makes the option
worth anything at all:

| a single-panel fault sees | |
|---|---|
| **today** (trunk T8A only) | **8A** — smoke long before it opens |
| **+ per-column T2A** | **2A** |
| **+ per-panel PTC** (options A/B) | ~1A, localised to one panel |

**The 8A → 2A step is the large one and costs zero board changes.** The PTC's
increment only bites in a narrow partial-fault window — a panel drawing 1–2A. A
dead short opens either device.

**Why deferred anyway:** stock SMX has no fusing below its converter either, and
this is a prototype with spare panels rather than a product shipped to strangers.
The parts are inline holders at the fan-out and can be added the day a fault
makes them look worthwhile — the same holder style is already purchased.

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
- **What this does NOT change:** the trunk's 2mm geometry (A keeps it via a new
  route, B/C keep it literally), RS-485/INT/FSR circuits, the brain, and the
  identical-panels invariant. **Resolved 2026-08-16:** the 2.7A→3.7A scare came
  from the per-channel misreading and is void. The class is **1.3A**, against
  ~3.9A for 2mm of 1oz outer copper at a 10°C rise — a 3× margin, the most
  comfortable this trunk has ever been.

## Decision checklist — CLOSED 2026-08-17

**Step 1 resolved: none of the above.** The board ships unprotected for rev A;
steps 2–4 were never started and no schematic or layout change exists. The
material below is kept only so a rev-B revisit does not start from zero.

**What actually has to happen instead — all procedural, all at build time:**

1. ✅ **Meter the PSU's captive-cable polarity before cutting the barrel off.**
   Both conductors are black and the moulded ridge's meaning is unrecorded, so
   the barrel is the only unambiguous reference — and cutting destroys it.
   Detail in `hardware/harness/12v-trunk.yml`.
2. ✅ **Engrave `+12V` and `GND` on the printed fan-out carrier.** The two rails
   are identical orange Wago blocks; this is the mitigation for the single
   highest-consequence error in the build.
3. ✅ **Meter the fan-out before first power-on.** One check, and it covers the
   whole-pad reversal case.
4. ✅ **Verify XT30 gender by inspection** — sockets on the supply side — and
   confirm each soldered half's polarity before mating.
5. ⏸ Per-column harness fuses remain available at any time. Not now.

### Notes for a rev-B revisit, if one ever happens

- **Option C is the cheapest board change** and its original "Cost 2" is void:
  it worried a connectors-only passthrough carrier would break, but
  `docs/MODULAR_PANEL_COUNT.md` describes the passthrough as a *separate,
  simpler board that has never been built* — it would simply omit the FET.
- **A trunk-level P-FET was never evaluated here** because this document
  predates the harness design. One part in the 12V trunk would protect all nine
  panels *and* the underglow, versus 20 on-board. At 6.34A it wants ≤10mΩ
  (~0.4W) in DPAK/TO-220; the wrinkle is that a discrete FET in a harness needs
  something to mount on, though the printed carrier could house it.
- If in the layout anyway, **add F201/Q201 as DNP footprints** — see the banner
  at the top.
