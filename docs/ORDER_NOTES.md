# Order notes — what to say to JLCPCB

Companion to `docs/PRE_ORDER_CHECKLIST.md`. That file is the *process*; this one
is the **text and answers you need while the order form is open**, including
everything JLC's DFM flags that is intentional.

**After the order:** `docs/CAM_REVIEW.md` reviews the production files JLC sends
back for approval — what their CAM changed versus what we uploaded, and the
process parameters read out of their own job files.

Generated against the 2026-08-18 packages, **regenerated after the RP2040
pad-width and R305/R306 part changes of that same day** (see §7). **Regenerate and
re-verify if the boards change** — see the checklist §1.

---

## Files to upload

| order | file |
|---|---|
| Panel, 4-layer + PCBA | `hardware/dual-panel/panel/production/panel-gerbers.zip` |
| ↳ its BOM | `hardware/dual-panel/panel/production/panel-BOM.csv` |
| ↳ its CPL | `hardware/dual-panel/panel/production/panel-CPL.csv` |
| Master, bare fab only | `hardware/master-pcb/production/master-pcb-gerbers.zip` |

⚠ **Upload the panel as a "panel by customer" / customer panel, NOT a single
board.** It carries **two different designs** (carrier + brain) joined by a rail
frame with mouse-bite tabs. JLC's own panelization only arrays one design, so
supplying it this way is what keeps it a single order — one engineering fee, one
PCBA setup, one stencil, one shipment.

## Order settings

| setting | value |
|---|---|
| Layers | **4** |
| Panel size | **227.57 × 143.10 mm** (326 cm²) |
| Quantity | **20** |
| Assembly | **Both sides, SMD only** — 101 top / 14 bottom, 115 placements |
| Through-hole | **NOT assembled** — all THT is hand-soldered by us |
| Via fill | **POFV (epoxy filled & capped) — REQUIRED, see below** |
| Build time | Slow (3–4 day assembly) |
| Confirm production file / parts placement | **Keep both** (~$1.50) and answer the emails same-day |
| Master board | **Bare fab only, no PCBA.** 4-layer, qty 5, **OSP** (see below) |

**Master board surface finish: OSP — decided 2026-08-18.** ENIG was priced at
**$17.10**, which more than **triples** the master's $8.04 cost. It was rejected
because the downside it insures against is cheaper than the premium: a master
re-fab is **$8.04 + ~$1.50 incremental shipping ≈ $9.54**. The fine-pitch argument
also does not hold here — the finest parts are **SOIC-8 (1.27 mm) and SOT-23-5
(0.95 mm)**, both comfortable hand-solder targets on any finish. **Mitigation for
the 3 shelf spares: keep them in JLC's original sealed bag with desiccant**, which
extends OSP life well beyond the 6–12 months quoted for handled, open-air boards.
⚠ **Do not pro-rata JLC's ENIG price by area** — a $1.22 area estimate came out
14× low, because the charge is mostly a minimum, not per-cm².

**Panel surface finish: ENIG — decided, not open.** The reasoning genuinely differs
from the master's: 20 boards, 30 bench-probed test points, machine assembly followed
by hand-soldered through-hole *weeks later*, QFN-56 in the reflow path, and spares
meant to last years. JLC's free OSP was evaluated and
rejected: OSP's flatness advantage is nil against ENIG, while this board has **30
bench-probed test points**, hand-soldered through-hole done *after* SMD assembly
across 20 panels, and spares meant to stay solderable for years. OSP degrades on
all three counts. The logo is **silkscreen**, so the finish is not an aesthetic
question at all.

---

## 1. POFV — say this explicitly

> Please **fill and cap the 0.30 mm drill vias**. The brain board has **83
> via-in-pad instances**, 17 of them under the RP2040 (U306). The **44 vias at
> 0.60 mm drill do not need filling, and none of them are in a pad.**

**This is not optional and not a cost trade.** Unfilled, solder wicks down those
83 vias during reflow and starves the joints — including the RP2040's.

**POFV cost — settled.** On the real **qty-20** quote it is **Via covering $32.44 +
Via plating method $4.89 = $37.33**, ~6.7% of the order
(`hardware/dual-panel/panel/QUOTE-2026-08-18.md`). At qty 5 it was $22.77. This
closes an earlier note here that claimed it had "never been priced".

## 2. J305 (USB-C) — flag proactively

> J305's through-hole contacts sit at **exactly 0.45 mm edge-to-edge**, which is
> at your published multilayer hole-to-hole floor. Please confirm this is
> manufacturable as drawn.

Our `.kicad_dru` deliberately relaxes the pad-to-pad rule to 0.45 mm for this
part only; the generic hole-to-hole rule stays at 0.5 mm. If JLC refuses, a
**vertical** USB-C footprint (`USB_C_Receptacle_LCK_TCF829D_TEMPLATE`) is still
in `dual-panel.pretty` as a deliberate revert path.

## 3. DFM findings that are INTENTIONAL

> ### ✅ Post-fix DFM re-run, 2026-08-18 — everything remaining is accepted
>
> **Cleared by the fixes:** the ~100 annular-ring Dangers (vias now 0.60 mm pad /
> 0.150 mm annular) and the flash trace-spacing Dangers (scoped QSPI pour rule).
>
> **What is left, all four categories accepted — the board is orderable:**
>
> | remaining | verdict |
> |---|---|
> | USB differential pair at ~0.15 mm | ⚠ **intentional — do NOT widen** |
> | tht to smd, 11 | not applicable to this order |
> | silkscreen warnings | cosmetic |
> | slot width | at JLC's stated minimum |
>
> ⚠ **The USB pair is the one to actively refuse to "fix."** USB± are 0.25 mm
> wide with ~0.14–0.15 mm spacing, routed as a coupled pair over the GND planes.
> **The gap is what sets the differential impedance** — widening it to satisfy a
> generic spacing check would detune the pair. It is also 0.15 mm against JLC's
> 0.09 mm minimum, so this is a margin advisory, not a violation.

If JLC queries any of these, the answer is "yes, as drawn":

| DFM flag | why it is fine |
|---|---|
| **Lead to hole distance** (~72) | Deliberate **via-in-pad** on the brain. POFV above is the mitigation. |
| **Pin left / right / outer edge** on U306 | ✅ **Fixed 2026-08-18 and confirmed cleared on the re-run.** Pads widened 0.20 → **0.23 mm** = the datasheet's **b max**, via the local `dual-panel:…_RPiPad` footprint. |
| **Pin inner edge** on U306 (42) | ⚠ **INTENTIONAL — accepted, do NOT "fix". See §8 for the full argument and the reply to send.** Our pad inner edge is at exactly 3.000 mm; the pin inner edge at the datasheet's **L max = 0.500 mm** is also exactly 3.000 mm. Zero heel *at worst case only* — at nominal L = 0.400 there is a proper 0.100 mm heel. Adding heel is measured to be worse, not better. |
| **Lead area overlapping pad** on U306's ground pad | Our EP is **3.2 × 3.2 mm, matching Raspberry Pi's own reference board**. Correct as drawn. |
| **tht to smd** (11) | **Seven are bare test-point probe holes** with nothing soldered into them; the other four (J213, J303, SW202's two lugs) are **hand-soldered by us**. JLC assembles SMD only here. Closest pair is 1.13 mm. |
| **Trace spacing 0.09 mm** | **Copper-pour-to-track**, not track-to-track, and at JLC's stated 0.09 mm minimum. Real routing is clear — tightest track-to-track is the USB± differential pair at 0.1377 mm. **The QSPI flash serpentine, which is what JLC's screenshot showed, is fixed** — a scoped `.kicad_dru` rule holds the pour ≥0.13 mm off those nets, so nothing QSPI remains under 0.15 mm. |
| **Slot width 0.5 mm** (6) | At JLC's stated minimum, not under it. |
| **Silkscreen over pads / holes** (96) | Cosmetic. Clip as needed. |
| **Negative soldermask expansion** (62) | Intentional — mask openings slightly smaller than pads. |
| **Fiducials "null"** | **Six exist** (`KiKit_FID_T/B_1-3`) at the panel corners; the detector did not recognise KiKit's. |

## 4. Re-run SMT DFM *with* the BOM and CPL

The SMT analysis is only meaningful with placement and part data uploaded.

## 5. BOM and placement checks

- ⚠ **Tick EVERY BOM row.** Rows left "to be confirmed" are **silently not
  assembled**. 35 lines, every one carrying an LCSC number.
- Review any yellow ⚠ substitution rows before ticking.
- **D301/D302 (PMEG3015EH) are DNP** — confirm they are **absent** from the BOM,
  the CPL and the placement preview. Their footprints stay on the board as a
  hand-solderable rescue path. **D303 is a different part and must be present.**
- Verify live stock on the extended parts: **C113952** (SMAJ5.0A — C87074 and
  C98802 are drop-in equivalents) and **C2671361** (THVD1450). ⚠ **The RS-485
  transceiver was THVD1429/`C1850236` until 2026-08-18** — if any older cart,
  quote or note still lists `C1850236`, it is stale.
- ⚠ **LCSC stock is not JLC assembly stock.** They are separate inventories; a
  part can be deep in stock at LCSC and still unavailable to PCBA. The matcher's
  number is the one that counts.
- **R305/R306 were moved to `C25190` on 2026-08-18 for JLC assembly stock**
  (was `C858950`). Confirmed against both LCSC and JLC part pages as
  **27 Ω ±1 %, 0603, 100 mW, Uniroyal 0603WAF270JT5E** — same value and tolerance,
  so the `27R 1%` schematic value still reads true. These are the USB D+/D− series
  resistors, where 27 Ω is the load-bearing number and ±1 % is already generous.

### Rotation / polarity sweep on the placement preview

JLC's renderer uses *their* library's tape-zero orientation, not KiCad's, so a
90°/180° preview error is a real placement error. Check pin 1 / polarity on:

**U306** (RP2040, QFN corner) · **U301–U305, U307, U308** · **X301** · **D303** ·
**D202** · **C308** (tantalum stripe) · **C201** (electrolytic) · **D201**
(SMA TVS cathode band) · **one WS2815 from each rotation group**.

⚠ **The WS2815 row of 3 is intentionally 180° from the row of 4** — that is the
serpentine layout, not a placement mistake. They use our custom PLCC6 footprint,
so JLC may render a generic body; **orient by pads against the part photo, not
the render.** An error there repeats ×25.

**Fix rotations in JLC's preview UI** (select + rotate), not by re-uploading the CPL.

### ✅ Done 2026-08-22 — placement confirmed, LED orientation verified

JLC's "Confirm parts placement" step was approved on **2026-08-22**, with the
**WS2815 orientation checked and correct** in the preview. That closes the
highest-consequence item in this section: the 180° rotation had **no file
representation** — it lived only as prose here and in the order remark — and an
error would have repeated 25× per board across 20 boards, which is not the
"rework a part or two" case every other item on the list is. Caught at the right
point, before manufacture rather than on arrival.

## 6. Final human pass

Page through JLC's gerber viewer — it renders the same artwork the fab uses.
Confirm: board outline and rail frame, mouse-bite tabs, layer order
(F / In1=GND / In2=GND / B), silk name + rev + year, logo copper/mask pair intact,
the **silkscreen logo**, **and a ground pour present on BOTH boards** (a missing brain pour is a real
defect this project has already shipped once — see `docs/DUAL_PANEL.md`).

Sanity-check against the **real qty-20 quote of 2026-08-18**
(`hardware/dual-panel/panel/QUOTE-2026-08-18.md`): dual-panel **PCB $174.47 + PCBA
$375.41**, master ×5 **$8.04**, **merchandise $557.92**. If a "qty 20" quote lands
near the old **qty-5** figure of $297.38, the quantity field did not take.

⚠ **The displayed total EXCLUDES "Advanced Options"** — bake $7.88 + cleaning $3.28
= **$11.16** is billed separately after review. Budget $642.09, not $630.93.

⚠ **CONFIRMED 2026-08-18 (post-order): a "two designs on one panel" supplement of
$24.42 USD / $33.91 CAD was charged after review.** The dual-panel carrier and
brain are two different designs in one panelised `.kicad_pcb`, and JLC bills for
that. **It was anticipated, and it is a real, recurring cost of the two-board
split — budget it into any rev-2 panelisation decision.** Not yet reconciled
against the invoice whether it subsumes the $11.16 advanced-options line.

⚠ **Editing the quantity WIPES both remark fields.** Set quantity first, then
paste §9a/§9b, then re-verify every option.

## 7. LCSC — same day

> ### ❌ There is NO combine option at LCSC checkout — tried 2026-08-18
>
> It was expected to appear (place JLC first, then pick it at LCSC checkout) and
> **it does not exist.** You pay LCSC shipping in full, then email
> `support@lcsc.com` afterwards. **The email below is the only route.**
>
> **What happened on the real order:** LCSC paid in full at **$216.33 CAD incl.
> $33.19 USD shipping**, then emailed immediately with both order numbers.
> ⚠ **Watch for a shipping recalculation / partial refund** — the LCSC shipping is
> already paid, so any saving comes back rather than being deducted up front.

Place **Order 2 (LCSC — $138.68 merchandise, $155.80 delivered, 30 lines)** first
or same-day, then email
`support@lcsc.com` with **both order numbers** to combine the shipment. Once
either order ships it is too late. Constraints: same currency and customer ID,
not to Mainland China, cannot be unbound afterwards, shipping recalculated.

---

## 7. Changes made 2026-08-18, after this file was first written

Both were made **pre-order, deliberately**, and both packages were regenerated
and re-verified afterwards (DRC 0 · unconnected 19 · parity 90 · ERC 0).

| # | change | why |
|---|---|---|
| 1 | **U306 RP2040 signal pads 0.20 → 0.23 mm wide** — local footprint `dual-panel:…_RPiPad` | Matches Raspberry Pi's official land pattern and should clear JLC's 98 pin-edge findings. Length stays 0.875 mm; centres, pitch and the 3.2 × 3.2 mm EP unchanged. |
| 2 | **R305/R306 `C858950` → `C25190`** | JLC assembly stock. Same part spec: 27 Ω ±1 %, 0603, 100 mW. |

Two knock-on fixes were required by change 1 and are already applied — a **zone
refill** (stale fill polygons produced three phantom pad-to-`GND_Brain` errors)
and **scoping the `QSPI flash pour clearance` rule to `B.Type == 'Zone'`** so it
stops policing track-to-pad. Full detail in `docs/PRE_ORDER_CHECKLIST.md`.

⚠ **Re-run the SMT DFM after uploading** — §3's accepted-findings table was
written against the pre-change board, and the pin-edge rows should now be gone.

---

## 8. "Pin inner edge" on U306 (42 Dangers) — accepted, with the evidence

This is the one remaining U306 finding after the pad widening, and it is
**deliberate**. Do not attempt to clear it.

### What it actually measures

RP2040 datasheet **Table 612, page 607** — the QFN-56 terminal dimensions:

| symbol | min | nom | **max** |
|---|---|---|---|
| **b** (terminal width) | 0.130 | 0.180 | **0.230** |
| **L** (terminal length) | 0.300 | 0.400 | **0.500** |

Body is **7 mm BSC**, so the body edge sits at **3.500 mm** from centre. At
**L max = 0.500** the pin's inner edge is at **3.000 mm**. Our pad's inner edge
is at **exactly 3.0000 mm**. Hence the reported value of **0 mm**: at worst-case
terminal length the pin inner edge and the pad inner edge are *exactly flush*.

**At nominal L = 0.400 the pad extends 0.100 mm inward past the pin** — a proper
heel. The finding only exists at the worst-case corner, which is what JLC models.

### Three reasons it is not a defect

1. **JLC's own checker already accepts this exact relationship elsewhere on this
   board.** The widened pads are **0.23 mm = b max exactly**, i.e. also perfectly
   flush with the worst-case pin — and the left / right / outer edge checks went
   **green**. Same geometry, opposite verdict. The inner-edge rule is simply
   stricter at the boundary.
2. **42 of 56 is the signature of a tie, not a defect.** All four sides of our
   footprint are geometrically identical — every pad verified at inner edge
   3.0000 mm, outer 3.875 mm. A real geometric problem would flag **56 or 0**.
   Flagging 42 means their comparison is tipping both ways on an exact 0.000 tie.
3. **QFN is bottom-terminated** — the joint forms *under* the terminal, so the
   heel fillet is an inspection aid, not a strength requirement. JLC's own wording
   is "can affect solderability **if excessive**". Zero is the boundary, not
   excessive.

### Adding heel was tested and is measurably WORSE

Extending all 56 pads 0.075 mm inward (length 0.875 → 0.95, outer edge held at
3.875) was built and DRC'd. It produces **10 violations**:

- **2 × `solder_mask_bridge`** — pad 51 (QSPI_SD3) and a +1.1VDC track sharing one
  mask aperture at **0.072 mm**. That is a real solder-short risk, i.e. trading an
  advisory for an actual defect.
- **6 × pad-to-via clearance** collapsing to **0.137–0.182 mm** against the 0.2 mm
  netclass rule.
- 2 × track-to-pad down to 0.072 mm.

Clearing those means re-routing the RP2040 escape — the most congested region on
the brain, carrying **17 via-in-pad**. The datasheet itself endorses the current
arrangement (page 607 note): *"the one on RP2040 is smaller than most… This gives
the opportunity to route between the central pad and the ones on the periphery."*
That is exactly what the brain does, and heel would eat that channel.

### Reply to send JLC if they query it

> The "pin inner edge" findings on U306 are **intentional and accepted**. Our land
> pattern's inner edge sits at 3.000 mm from package centre, which is exactly the
> pin inner edge at the datasheet's **maximum** terminal length L = 0.500 mm
> (RP2040 datasheet Table 612). At nominal L = 0.400 mm there is a 0.100 mm heel.
> The RP2040 is a bottom-terminated QFN, so the joint forms under the terminal and
> the heel fillet is not load-bearing. We cannot extend the pads inward: that
> region carries the via-in-pad escape routing, and doing so creates solder-mask
> bridging. **Please proceed as drawn.**

---

## 9. The order-form notes boxes — paste-ready

⚠ **There are TWO separate remark fields and they go to different teams.** The PCB
remarks are read by the fab/CAM engineer (panelization, drilling, artwork); the
PCBA remarks are read by the assembly team (sourcing, placement, orientation,
baking, cleaning). Putting an assembly instruction in the PCB box means nobody who
needs it will read it.

⚠ **Changing the quantity WIPES the remarks.** Set quantity first, then paste.
Re-check the whole form afterwards — other settings can reset too.

### 9a. PCB remarks (bare-board fabrication)

```
1. CUSTOMER PANEL - please do not re-panelize. This panel carries TWO DIFFERENT
   designs (a carrier board and a brain board) joined by a rail frame with
   mouse-bite tabs. Please fabricate exactly as supplied, one panel per unit.

2. VIA FILL (POFV): please epoxy-fill and cap the 0.30 mm drill vias. There are
   83 via-in-pad instances, 17 of them under the RP2040 (U306). The 44 vias at
   0.60 mm drill do NOT need filling, and none of them are in a pad.

3. PLEASE CONFIRM BEFORE STARTING: J305 (USB-C, GCT USB4085-GF-A) has
   through-hole contacts at 0.45 mm edge-to-edge, which is at your published
   multilayer hole-to-hole minimum. Please confirm this is manufacturable as
   drawn.

4. The following DFM flags on the artwork are INTENTIONAL. Please fabricate as
   drawn and do not "optimize" them:
   - USB differential pair (~0.25 mm trace, ~0.14 mm gap). The gap sets the
     differential impedance; widening it would detune the pair.
   - Negative soldermask expansion (mask openings slightly smaller than pads).
   - Slot widths at 0.50 mm, which is your stated minimum.
   - Silkscreen overlapping pads is cosmetic - please clip as needed.

5. FIDUCIALS ARE PRESENT: six, at the panel corners. If your checker reports
   "fiducial: null" it has not recognised them; please use the ones supplied
   rather than adding new ones.
```

### 9b. PCBA / assembly remarks

```
1. SMD ONLY - please do NOT assemble any through-hole parts. All THT is hand-
   soldered by us, so any "THT to SMD clearance" flags are not applicable.

2. DO NOT POPULATE D301 and D302. Their footprints are on the board deliberately
   as a hand-solderable rescue option. IMPORTANT: D303 is the same part number
   (PMEG3015EH) but MUST be populated.

3. COMPONENT BAKING: the 25x WS2815 LEDs (C5446699) are MSL 5a with a 24 h floor
   life. Please bake before reflow.

4. PLACEMENT: the WS2815 LED row of three is intentionally rotated 180 degrees
   relative to the row of four (serpentine LED layout). Please do not "correct"
   this. It repeats 25x per board.

5. "Pin inner edge" on U306 (RP2040) is INTENTIONAL. Our land pattern's inner
   edge sits exactly at the pin inner edge for the datasheet's MAXIMUM terminal
   length (L max = 0.50 mm, RP2040 datasheet Table 612). Correct as drawn.

6. CLEANING-SENSITIVE PARTS (board cleaning is selected):
   - SW301 (Omron B3U-1000P tactile switch, C231329) - the manufacturer's
     datasheet states "Washing: Not possible" and rates it IEC IP40, i.e. no
     liquid ingress protection. Please avoid liquid ingress at this part, or
     advise if that is not possible with your cleaning process.
   - X301 (ABM8-272-T3 crystal) - NO ULTRASONIC CLEANING. Standard aqueous
     cleaning is acceptable; it is the ultrasonic energy that is the concern.
   - C201 (470 uF aluminium electrolytic, 10 x 10.5 mm can) - please avoid
     prolonged immersion; fluid trapped under the can and at the rubber seal
     is the concern.
   - D202 and D203-D227 (WS2815 LEDs) - silicone/epoxy encapsulant, please
     avoid aggressive solvents.
```

### If you need to trim it

**Never drop these five.** Each changes what JLC physically does, and each has a
failure mode that is unrecoverable once the run starts:

| note | if omitted |
|---|---|
| **PCB 1** — customer panel | they array one design, or reject the file |
| **PCB 2** — POFV | solder wicks down 83 vias and starves the joints, incl. the RP2040's |
| **PCB 3** — J305 confirm | you find out it is unmanufacturable *after* fabrication |
| **PCBA 1** — SMD only | unexpected THT assembly charges, or a query that stalls the order |
| **PCBA 2** — DNP D301/D302 | **the power-OR is wrong on all 20 boards**, and D303 shares the part number |

Everything else is recoverable or cosmetic. **PCBA 3** (baking) is already a paid
line item on the quote, **PCBA 6** (cleaning) only matters because board cleaning is
selected, and the "intentional DFM" items can be re-stated in the confirmation
emails if JLC queries them — which is what "Confirm production file" and "Confirm
parts placement" are for.

### Master board order — no notes needed

Bare fab only, 4-layer. **No PCBA, no BOM, no CPL, no baking, no POFV.** The
`docs/BOM.md` "qty 2" is the *need*; **order qty 5**, JLC's minimum, which is what
the ≈$8 quote was against.

---

## 10. Board cleaning — the sensitive-parts note

JLC asks you to name cleaning-sensitive parts when you select board cleaning
($0.82 on the 2026-07-31 quote).

### The scope is much smaller than it looks

**JLC assembles SMD only on this order**, so every classically cleaning-hostile
part is *not on the board* when it is cleaned — they are hand-soldered by us
afterwards. That excludes **SW201** (the open-frame 4-pos DIP slide switch, which
would otherwise be the single worst part here), **SW202** (DPDT slide), **J305**
(USB-C) and all 19 other connectors.

Only **six SMD part types** are exposed. Four are worth naming:

| ref | part | risk | verdict |
|---|---|---|---|
| **SW301** | Omron **B3U-1000P** tactile switch (BOOTSEL), C231329 | 🔴 **Manufacturer says NO.** Omron's own datasheet: **"Washing: Not possible"**, degree of protection **IEC IP40** — IP4**0** means *zero* liquid protection. | **Must be named.** |
| **C201** | 470 µF aluminium electrolytic, 10 × 10.5 mm can, C82014 | 🟠 Large can on standoffs — fluid wicks underneath and sits there, and the rubber end-seal is not solvent-proof. Standard "limit immersion" part. | Name it. |
| **D203–D227** ×25 + **D202** | WS2815 PLCC6 + 0603 debug LED | 🟡 Silicone/epoxy encapsulant in a reflector cup; aggressive solvents can craze or cloud the lens. Low risk with DI water, and it repeats **25× per board across 20 boards**. | Name as "no aggressive solvents". |
| **X301** | ABM8-272-T3 crystal, C20625731 | 🟡 Seam-sealed ceramic package, so **aqueous cleaning is fine**. The real hazard is **ultrasonic** — cavitation couples into the quartz blank. | Name it as **"no ultrasonic"**, not as "sensitive". |

Everything else — all resistors, MLCCs, the tantalum C308, and every IC including
the RP2040 QFN, the flash, the transceiver and the TVS — has no cleaning concern.

### Suggested wording

```
CLEANING-SENSITIVE PARTS:

- SW301 (Omron B3U-1000P tactile switch, C231329) - the manufacturer's
  datasheet states "Washing: Not possible" and rates it IEC IP40, i.e. no
  liquid ingress protection. Please avoid liquid ingress at this part, or
  advise if that is not possible with your cleaning process.
- X301 (ABM8-272-T3 crystal) - NO ULTRASONIC CLEANING. Standard aqueous
  cleaning is acceptable; it is the ultrasonic energy that is the concern.
- C201 (470 uF aluminium electrolytic, 10 x 10.5 mm can) - please avoid
  prolonged immersion; fluid trapped under the can and at the rubber seal
  is the concern.
- D202 and D203-D227 (WS2815 LEDs) - silicone/epoxy encapsulant, please
  avoid aggressive solvents.
```

### If JLC says they cannot protect SW301

**Cleaning is still probably worth keeping.** The board has **30 bench-probed test
points**, and flux residue is exactly what degrades probing — one of the three
reasons ENIG was chosen over OSP. Against that, a BOOTSEL switch that goes
intermittent is **not fatal**: **J209 (SWD) is the firmware-independent recovery
path**, and it is through-hole so it is not on the board during cleaning either.

Judgement call, but the asymmetry favours cleaning: 30 test points × 20 boards
that you *will* probe, against one rarely-used switch that has a documented
backup. If you'd rather not risk it, deselecting board cleaning costs $0.82 and
loses the flux benefit.

---

## 11. Master board — DFM findings (run 2026-08-18, the first time ever)

DFM had **never been run on the master**. It was, and it found the same
annular-ring problem the panel had already been fixed for.

| finding | verdict |
|---|---|
| **Annular ring too small** | 🔴 **WAS REAL — now fixed.** 297 vias sat at 0.45 mm pad / 0.30 mm drill = **0.075 mm annular**. All enlarged to **0.60 mm → 0.150 mm**, matching the panel. Re-verified 0/0/0/0. **Re-run DFM on the regenerated gerbers.** |
| **THT to SMD** | ✅ **Inapplicable — the master is bare fab, no PCBA at all.** Not "SMD only" like the panel: *nothing* is assembled. Every part is hand-soldered by us. Ignore it. |

⚠ **The master gerbers were regenerated** — re-upload
`hardware/master-pcb/production/master-pcb-gerbers.zip`. The old zip has the
0.45 mm vias.

⚠ **Lesson: a quote is not a DFM check.** `docs/PRE_ORDER_CHECKLIST.md` had carried
"the master quoted fine at 0.075 mm" as partial reassurance for months. JLC will
happily quote a board it would then flag.

## 12. Post-order confirmation emails — what actually came back

### 2026-08-20 — JLC queried the baking selection (RESOLVED, replied same day)

JLC (order `SMT026081862839`) wrote that the **component baking
service was selected but their system identified no component in the order that
requires baking**, and asked which JLCPCB part number to bake.

**The §9b note 3 was not wrong — it was simply not on the automated path.** The
remark fields are read by a human at review time; the MSL check runs off JLC's
parts database, and their entry for the **WS2815 (`C5446699`)** is not tagged
moisture-sensitive. Ticking the checkbox conveys *that* you want baking, never
*which part*.

**Replied: bake `C5446699` only** (25× per board, D203–D227), on the MSL 5a /
24 h floor-life grounds already in §9b note 3, and accepted the charge.
**Price confirmed: $7.71 per component KIND, 48 h at 60 °C** — one kind here, so
in line with the ~$7.88 advanced-options line in §6.

### The generalisable rule

**Intent encoded in the FILES needs no remark; intent that is a PROCESS does.**

- `panel-BOM.csv` (37 lines) and `panel-CPL.csv` (115 placements) contain **no
  `D301`/`D302` and no through-hole part at all**, and `D303` is present and
  unambiguous. So **PCBA 1 (SMD only) and PCBA 2 (DNP) are belt-and-braces** —
  JLC assembles BOM ∩ CPL, and that set is already correct.
- **Baking, the cleaning-sensitive parts list (§10) and the "180° rotation is
  intentional" placement note have no file representation.** They exist only as
  prose, which is exactly why baking is the one that got queried. Expect the
  other two to be the next things asked about.
