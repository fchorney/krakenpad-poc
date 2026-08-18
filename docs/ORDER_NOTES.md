# Order notes — what to say to JLCPCB

Companion to `docs/PRE_ORDER_CHECKLIST.md`. That file is the *process*; this one
is the **text and answers you need while the order form is open**, including
everything JLC's DFM flags that is intentional.

Generated against the 2026-08-18 packages. **Regenerate and re-verify if the
boards change** — see the checklist §1.

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
| Master board | **Bare fab only, no PCBA.** 4-layer, qty 5 |

**Surface finish: ENIG — decided, not open.** JLC's free OSP was evaluated and
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

**Ask at quote time:** the POFV surcharge at 4 layers. It has never been priced
against a live quote.

## 2. J305 (USB-C) — flag proactively

> J305's through-hole contacts sit at **exactly 0.45 mm edge-to-edge**, which is
> at your published multilayer hole-to-hole floor. Please confirm this is
> manufacturable as drawn.

Our `.kicad_dru` deliberately relaxes the pad-to-pad rule to 0.45 mm for this
part only; the generic hole-to-hole rule stays at 0.5 mm. If JLC refuses, a
**vertical** USB-C footprint (`USB_C_Receptacle_LCK_TCF829D_TEMPLATE`) is still
in `dual-panel.pretty` as a deliberate revert path.

## 3. DFM findings that are INTENTIONAL

If JLC queries any of these, the answer is "yes, as drawn":

| DFM flag | why it is fine |
|---|---|
| **Lead to hole distance** (~72) | Deliberate **via-in-pad** on the brain. POFV above is the mitigation. |
| **Pin inner / left / right edge** on U306 | RP2040 uses KiCad's **IPC-generic `QFN-56-1EP_7x7mm_P0.4mm_EP3.2x3.2mm`**. Pads are 0.875 × 0.20 mm vs Raspberry Pi's official 0.80 × 0.23 mm — 0.03 mm narrower. Accepted. |
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
  C98802 are drop-in equivalents), **C1850236** (THVD1429).

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

## 6. Final human pass

Page through JLC's gerber viewer — it renders the same artwork the fab uses.
Confirm: board outline and rail frame, mouse-bite tabs, layer order
(F / In1=GND / In2=GND / B), silk name + rev + year, logo copper/mask pair intact,
the **silkscreen logo**, **and a ground pour present on BOTH boards** (a missing brain pour is a real
defect this project has already shipped once — see `docs/DUAL_PANEL.md`).

Sanity-check the total against the qty-20 band. The recorded **qty-5** baseline
is **$297.38**; if a qty-20 quote lands near that, the quantity field is wrong.

## 7. LCSC — same day

Place **Order 2 (LCSC, $142.93, 30 lines)** first or same-day, then email
`support@lcsc.com` with **both order numbers** to combine the shipment. Once
either order ships it is too late. Constraints: same currency and customer ID,
not to Mainland China, cannot be unbound afterwards, shipping recalculated.
