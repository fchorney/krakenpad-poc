# Panel PCB — pre-order checklist (rev 1.0, created 2026-07-18)

Run this list top to bottom before paying JLCPCB. Items marked ⬜ are open;
check them off in this doc (or delete the line) as they close.

**Build scope: 2 fully assembled pads** = 2 master PCBs + **20 panel PCBs**
(2 × 9 + 2 spares). Every quantity in this doc and in `docs/BOM.md` is sized
to that; if the scope ever changes, both docs move together.

## Footprint reconciliation for the LCSC parts (opened 2026-07-25)

Adopting the LCSC cart (`docs/BOM.md` order 2) swapped several parts for ones
with different land patterns. **All closed as of 2026-07-26** — two needed new
footprints (SW3, J1), one adopted the vendor pattern (J9/J4), three confirmed
as drop-in (RN1, both DIP switches).

**Method (2026-07-25):** footprints pulled with `easyeda2kicad` (free pip CLI,
no EasyEDA subscription) straight from LCSC by C-number:
`easyeda2kicad --footprint --lcsc_id=Cxxxxxx --output <dir>/lcsc`. Pulled parts
staged in `panel-pcb.pretty/`.

**Confirmed via easyeda2kicad — existing footprint fits, NO change needed:**
- ✅ **RN1** C840655 (4610X): pulled = SIP-10 2.54mm, pin-1 marker — identical
  to `R_Array_SIP10`. Only confirm the schematic wires RN1 pin 1 = the common
  bus (it does).
- ✅ **panel SW1 DIP-4** C52177925 (DS-04): pulled = 8 pads, 2.54mm, 7.62mm
  rows — matches `SW_DIP_SPSTx04…W7.62`.
- ✅ **master SW1 DIP-3** C46595747 (DS-3P-BU): pulled = 6 pads, 2.54mm, 7.62mm
  rows — matches `SW_DIP_SPSTx03…W7.62`.
- ✅ **J9 + J4** C8465 (WJ500V): **CLOSED 2026-07-26 by adopting the vendor land
  pattern** `TerminalBlock_WJ500V-5.08-2P` (1.30mm holes, 2.00mm pads, vendor 3D
  model) on BOTH boards, re-placed and re-routed; the hand-built
  `MRR52-5.08-2P` (1.5mm holes, Adam Tech drawing) is retired. Pitch 5.08mm and
  body 10.16×10.16mm matched our numbers. Left for arrival: eyeball pin-1
  orientation and body-silk clearance vs neighbours.

**New footprints — DONE 2026-07-26, both swapped, re-placed, re-routed, re-DRC'd:**
- ✅ **SW3 DPDT** → `panel-pcb:SW_SS22E01L5` (built from C609835 EasyEDA data;
  6 pads 2.5/2.5mm). Pin numbering 1‑2‑3 / 4‑5‑6 matched, so nets were unchanged;
  the **symbol was renamed EG2201A → `SS22E01L5`** (lib + schematic cache +
  lib_id) and the vendor's two **mounting lugs are now pads 7/8, tied to GND**
  (user-confirmed against the datasheet: 2/5 are the pole commons, 1/4 left,
  3/6 right). `SW_SS-22F04` deleted (was moot). Vendor 3D model attached.
- ✅ **J1 USB-C** → `panel-pcb:USB_C_Receptacle_LCK_TCF829D_TEMPLATE`
  (C53184807 has **no** EasyEDA data — hand-built from the datasheet, then
  **refined by the user in KiCad 2026-07-25**: 8 signal pins staggered at
  y=±0.825, VBUS/GND as **shared A/B posts** (A1+B12 in one hole, A12+B1 in
  another), 4 mounting posts; pad names A1..B12+SH still match the J1 symbol so
  the net mapping is correct. Swapped, re-placed, re-routed (U7 ESD + R3/R4 27Ω
  front end), DRC 0. All 8 GND pads were set to **solid zone connection** —
  at 1mm pitch with a 0.5mm thermal gap a second spoke cannot fit, and the SH
  post copper already merges with A12/B1. The name keeps the `_TEMPLATE` suffix
  for continuity; it is no longer a template. **3D model is a placeholder from a
  different part (CMUCF661016C) — visual bulk only, NOT valid for fit checks.**
  Unused fallback, if the part ever disappoints: right-angle C49302689
  (GT-USB-7107B), clean verified all-THT footprint already pulled
  (`USB-C-TH_GT-USB-7107B`), +~$5 total, orientation "free".

After each new footprint: re-place + re-route the ref, re-run ERC/DRC, keep 0/0.

## Layout rework closed 2026-07-24

Verified against the board after the fact, not just claimed:

- ✅ **Diff pairs re-routed for the real stackup** — USB now W=0.25/S=0.15
  (~90Ω), RS-485 W=0.15/S=0.2 (~119Ω). Details and the why in
  `docs/PANEL_PCB.md`.
- ✅ **Both pairs length-matched** — USB skew 0.023mm, RS-485 skew 0.019mm
  (via barrels included).
- ✅ **Review F6 closed** — QSPI_SD3 tuning profile `min_spacing` 0.07 → 0.15,
  so the meander's same-net gap is no longer 0.09mm.
- ✅ **TP13 (`RS485+`) / TP14 (`RS485-`) added** — 14 probe holes now.
- 🚫 **Review F7 / F8 are WONTFIX** — the one-sided B.Cu hops on USB D+ and
  RS485+ are deliberate crossovers (pairs arrive in the wrong order for their
  destination pins). See `docs/PANEL_PCB.md`; expect every future automated
  review to re-flag them.
- ✅ **Review F9 closed** — the `.dru` pad-to-pad hole rule corrected to JLC's
  actual 0.45mm floor (not suppressed); `hole_to_hole` enforcement restored.
  J1 remains zero-margin → DFM confirmation at order time.
- ✅ **Master PCB RS-485 done too** — W=0.15/S=0.2, F.Cu only, no vias,
  45.39mm both legs (**0.000mm skew**). Master DRC is **0 violations even with
  its three ignored rules lifted**, 0 unconnected, ERC 0; only 3
  `footprint_filters_mismatch` naming notes (Euroblock, WJ500V terminal, DIP-3). The
  master's ignore list does **not** include `hole_to_hole` — that suppression
  is panel-only. Details in `docs/MASTER_PCB.md`.

**Last audited against the design files: 2026-07-24** (post-rework staleness
sweep — J9 pinout, ERC/DRC status, placement counts, DNP handling, and the
re-export delta were all re-derived from the KiCad files, not carried forward).

## 1. Physical part verification (needs parts in hand)

- ⬜ **J9 (INT screw terminal)**: **2P** (swapped 2026-07-20 — true 1P KF301
  barely exists). **Pin 1 = INT signal (net `Net-(D30-K)`), pin 2 = dedicated
  GND** as of 2026-07-24 — this supersedes the earlier both-pins-bridged
  arrangement, so the wire is no longer position-agnostic. A single-conductor
  cable lands on pin 1 and leaves pin 2 empty; pin 2 exists to pre-provision a
  paired signal+GND return if the bench shows spurious triggers. Part: **KANGNEX
  WJ500V-5.08-2P**, LCSC **C8465** (2026-07-26; superseded Adam Tech
  MRR522-5.08-V, which superseded the KF301 pick). Footprint
  `TerminalBlock_WJ500V-5.08-2P` is the **vendor's own** land pattern, hole
  Ø1.30, pad 2.00. Pitch and body already match on paper — on arrival just
  confirm **which physical position is pin 1** (it is no longer
  position-agnostic) and that the pins pass the 1.30mm holes.
- ⬜ **FSR leads vs J3/J4/J6/J7**: mate a real FSR lead's JST PHR-2 plug
  against a B2B-PH-K top-entry header (or at minimum compare datasheet drawings
  pin-for-pin). Flagged 2026-07-10, never physically verified.
- ✅ **U8 (LM66200) package CONFIRMED 2026-07-26 — no parts needed.** Pulled
  C3235556 via easyeda2kicad: LCSC's own footprint is
  `SOT-583-8_L2.1-W1.2-P0.50-LS1.6-BL` — **8 pads, 0.50mm pitch, 2.1×1.2mm body,
  1.6mm lead span**, i.e. the 8-pin DRL, not the 6-pin variant. Land pattern vs
  the KiCad `SOT-583-8` the board uses: identical pitch and pad size
  (0.28×0.68 vs 0.30×0.67), rotated 90° by library convention, row separation
  1.28 vs 1.48mm — ~0.1mm of toe-vs-heel fillet allocation, normal between
  library sources, leads land inside both. No footprint change.
  U8 is SMD and not DNP so **JLC places it** — it is not a hand-solder cart item.
- ⬜ **D12/D23 DNP handling** (order-time BOM/CPL check, not a parts check):
  confirm the regenerated BOM/CPL **exclude** them while their footprints stay on
  the board — the hand-solder Schottky-OR rescue depends on the pads existing.
- ⬜ **D30 (SMAJ5.0A, new 2026-07-24)**: LCSC **C113952** written into the
  schematic — an *extended* part, so it adds a feeder/handling line to the
  quote. Confirm live stock in the JLC BOM dialog and re-pick if short
  (C87074 Diodes, C98802 ST are the same part in the same DO-214AC body).
- ✅ **SW1 (DIP-4) and SW3 (DPDT) CLOSED 2026-07-25/26 — nothing left to verify
  on arrival.** Both were settled from the vendors' own EasyEDA data, not
  guessed: panel SW1 (Zhongdi DS-04, **C52177925**) pulled as 8 pads / 2.54mm
  pitch / **7.62mm rows**, matching `SW_DIP_SPSTx04…W7.62`; master SW1 (DORABO
  DS-3P-BU, **C46595747**) as 6 pads / 2.54 / 7.62 matching the x03 variant; and
  SW3's footprint (`panel-pcb:SW_SS22E01L5`) was *built* from C609835's vendor
  data with the pole grouping user-confirmed against the datasheet (2/5 = pole
  commons, 1/4 left, 3/6 right, lugs = pads 7/8 → GND). The earlier "verify row
  spacing on arrival" line predated that work.
  **SW1 source RE-SETTLED 2026-07-25: Zhongdi `DS-04`, LCSC C52177925** (30/$6.03),
  riding the LCSC order the pivot created. The note below is the superseded
  2026-07-24 reasoning, kept for context:
  **~~SW1 source SETTLED 2026-07-24: CUI `DS01C-254-S-04BE` from DigiKey~~**
  ($0.70 ea, 7,376 in stock at decision time; 20 pcs = ~$14). It rides on the
  panel-THT DigiKey order that's happening anyway. The earlier LCSC pick
  (YE DSWB04LHGET, C99418, ~$0.12) is **dropped** — there is no LCSC order to
  attach it to, so it would mean a separate shipment to save ~$12. The
  AliExpress 10-packs are likewise dropped.
- ✅ **WS2815 datasheet-variant confirm** (human review finding 1.a, closed
  2026-07-19): LCSC C5446699 confirmed = WS2815B-V1, the exact part of the
  WS2815B-V1 V2.0 datasheet (VIH abs 2.7V min / input abs-max 5.7V — closed
  the reviewer's "must shift to 12V" finding; 12V would violate abs-max).
  Optional extra insurance only: bench-drive a WS2815 strip from the
  prototype's SN74AHCT125N at 5V (we've only personally tested WS2812B).
  **WAIVED 2026-07-26** — not worth buying WS2815s just to test. The datasheet
  confirm above is the substantive check; the 5V-shifter question rests on
  VIH 2.7V min, which is a spec guarantee, not a marginal reading. Residual
  risk is accepted: if a bring-up board shows flaky LED data, the shifter rail
  is the first suspect.
- ✅ **Per-LED pin-1 caps (C22–C49) are vendor-sanctioned — CLOSED 2026-07-26.**
  **Two** Worldsemi documents that bracket our revision in time — the original
  2018-era **WS2815** doc (`led-stuebchen.de/download/WS2815.pdf`, the same doc
  NORMAND hosts) and **WS2815B-V3** (`ledlightinghut.com/files/WS2815B.pdf`) —
  both give pin 1 verbatim as *"VCC … IC POWER SUPPLY, Suspended or connected
  with a filter capacitor to GROUND"*. Our **V1 V2.0 doc is the outlier**, the
  only one calling it "NC / Suspended PIN"; it is also the only one labelling
  pins 4/6 "DIN1/DIN2" instead of "DIN/BIN", so that column looks edited from a
  different source. Caveat kept deliberately: paper does **not** prove V1
  silicon brings the rail out to pin 1 — but a 100nF is no DC load, so if V1 is
  truly NC the cap is merely inert. Safe either way; only probing a powered
  part (≈5V vs floating) would settle it outright. **No per-LED VDD decoupling is wanted**: V3 states the part
  needs "NO extra components", neither revision contains an application
  circuit, and the channels are constant-current (~10–12mA fixed) so there is
  no per-pixel switching transient to decouple. Leave C22–C49 as drawn.
  Watch-out: V3 silicon uses VIH = 0.7·VDD (~3.5V) vs V1's 2.7V absolute — the
  5V shifter clears both, but don't accept a V3 substitution silently.

## 2. Design-file state (all scriptable/checkable from the repo)

- ✅ **ERC genuinely 0** — re-verified 2026-07-24 on a scratch copy with
  `kicad-cli sch erc --severity-all`: 0 violations, and `erc_exclusions` in
  the project file is **empty**. The old "…with the cached-symbol
  `lib_symbol_mismatch` warnings excluded" caveat is dead — those were fixed
  for real on 2026-07-21, not suppressed. Re-run after any schematic edit.
- ✅ **Schematic changes pushed into the PCB** (verified 2026-07-20): U8 present
  as `Package_TO_SOT_SMD:SOT-583-8`, all 12 test points converted to
  `TestPoint_THTPad_D2.0mm_Drill1.0mm`, and every copper zone re-pointed at the
  renamed rails (`+3.3VDC`, `+5VDC`, `+12VDC`, `GND`). Zones refill clean.
- ✅ **Zone display names fixed 2026-07-24** (renamed in the GUI) — all eight
  now read as their actual net/function: `+3.3VDC Regulator Thermals` (F.Cu),
  `+5VDC Regulator Thermals` (F.Cu), `GND`, `+3.3VDC FSR` (In2, the analog
  pour), `+3.3VDC Main` (In2, board-wide), `+5VDC VBUS`, `+5VDC`, `+12VDC`.
- ✅ **AMS1117 output node labelled `/+5VDC_AMS`** (2026-07-24) — it used to
  ride auto-net `Net-(D23-A)`, named after a **DNP** diode, which is why it
  read as confusing. The auto-net is gone from the board entirely. Any older
  doc or review text citing `Net-(D23-A)` is stale.
- ✅ **DRC genuinely 0 / 0 unconnected / 0 schematic-parity** — re-verified
  2026-07-24 with `kicad-cli pcb drc --severity-all --schematic-parity`, and
  `drc_exclusions` is **empty**, and as of 2026-07-24 the rule-severity
  `ignore` list is down to **two purely cosmetic entries**:
  `footprint_filters_mismatch` and `footprint_type_mismatch` (library naming
  patterns — the custom WS2815 footprint vs the stock symbol's `LED*WS2812*`
  filter ×25, the MRR52 terminal, the DIP-4). `hole_to_hole`,
  `tuning_profile_track_geometries` and `missing_courtyard` are all **enforced
  again**.

  **Review F9 (J1 hole spacing) is CLOSED by rule correction, not by
  suppression.** J1's USB-C contact holes sit at 0.45mm edge-to-edge — exactly
  JLC's published multilayer floor — while the imported JLC `.dru` demanded
  0.5mm. The fix was surgical: **only** the `pad to pad clearance (with hole,
  different nets)` rule was relaxed to 0.45mm; the generic `hole to hole
  clearance (different nets)` rule stays at 0.5mm and still guards via↔pad and
  every other non-pad-pair hole.
  - ⚠ **The two rules are not interchangeable.** Both carry a `hole_to_hole`
    constraint, but the pad-to-pad rule is more specific and appears later, so
    *it* is what governs J1. Verified empirically: relaxing only the generic
    rule brings all 14 violations straight back. Don't "simplify" these two
    rules into one.
  - J1 still has **zero margin** against the fab limit — keep it on the
    DFM-confirmation list at order time.

  **Full audit 2026-07-24**, `--severity-all --schematic-parity
  --refill-zones` with the remaining ignores lifted: **0 violations, 0
  unconnected, ERC 0.** Only cosmetic `footprint_filters_mismatch` parity
  notes remain. That is the real state of the board with nothing suppressed.

  The old "known benign warnings, do not chase"
  list (48 silk self-clips, 25 `lib_footprint_mismatch`, 2 J1 silk-vs-edge)
  no longer applies — all 77 were fixed and the 35 stale exclusions removed on
  2026-07-21. If any of those reappear, treat them as new, not as known noise.
  The logo clearance issue is **CLOSED**: the F.Cu logo polygons were deleted
  and only the F.Mask polys kept, so the mask opening exposes the existing F.Cu
  GND pour instead of a separate copper island (coverage confirmed by
  point-in-polygon against the filled zone).
  **Surface-finish note now matters more:** the logo is exposed *ground* copper,
  so HASL gives silver and ENIG gives gold — see section on finish.
- ⬜ **Re-export gerbers/BOM/CPL is now MANDATORY, not conditional** — the
  files in `hardware/panel-pcb/production/` are **STALE**. Changes since the
  2026-07-18 export: U2 THVD1419→THVD1429, 52 MPN + 22 Datasheet properties,
  J9 1P→2P (new footprint + placement), rail/net renames (4.e/4.k), U8
  LM66200 added with D12/D23 → DNP, and 12 test points converted from SMD pads
  to THT probe holes (**a drill-count change — the drill file must be
  regenerated, not just the gerbers**). Plus the 2026-07-24 post-review rework:
  **D30 SMAJ5.0A added** (new SMA footprint, bottom side, LCSC C113952), **J9
  pin 2 re-netted signal → GND**, **R17 100R** as the INT series element, and
  **U8 bypass caps C54/C56 (1µF) + C55 (100nF)**. Commands are in git history;
  re-zip after.
- ⬜ Working tree committed and pushed.

## 3. JLC upload & BOM matching

- Files: `production/panel-pcb-gerbers.zip`, `panel-pcb-BOM.csv`,
  `panel-pcb-CPL.csv`. BOM carries LCSC numbers (from the 2026-07-18 quote
  session, `tmp/bom.xls`) — every line should auto-match exactly.
- ⬜ All BOM rows **confirmed** (checkbox ticked). Rows left "to be confirmed"
  are silently NOT assembled. Target: every line checked (D1 debug LED
  included unless deliberately dropped — decide before ordering).
- ⬜ Any substitution rows (yellow ⚠) reviewed before ticking.
- ⬜ Stock check: if a matched part shows insufficient stock, re-pick and
  note the new C-number back into the schematic's LCSC field afterward.
  ("Qty" = pieces your order needs; "My Inventory" 0 is normal.)

## 4. Placement preview (JLC order page)

- ⬜ Rotation/polarity sweep — JLC's renderer uses their library's tape-zero
  orientation, not KiCad's; 90°/180° preview errors are real placement errors.
  Check pin-1/polarity on: U1 (QFN corner), U2–U7, X1, D29, D1,
  C38 (tantalum stripe), C51 (electrolytic), D30 (SMA TVS cathode band — new
  2026-07-24), and one WS2815 of each rotation group (a library error repeats
  ×25). **D12/D23 are DNP** (`attr smd dnp`, verified in the PCB) so JLC never
  places them — nothing to check there, but do confirm they're absent from the
  placement preview.
- ⬜ WS2815s use our custom PLCC6 footprint — JLC may render a generic body;
  orient by pads against the part-detail photo, not the render.
- ⬜ C51: confirm 10mm can diameter sits correctly on the pads.
- ⬜ Fix any rotation issues in their preview UI (select + rotate), not by
  re-uploading the CPL.

## 5. Order options

- ⬜ 4-layer, 127×127mm, standard via class (0.3/0.45 — already rule-checked;
  no premium process options needed).
- ⬜ **Surface finish**: HASL = silver exposed-copper logo, ENIG (+$) = gold.
  Aesthetic choice, decide deliberately.
- ⬜ Assembly: **both sides**. Current count (2026-07-24, from the PCB, DNP
  excluded): **113 placements — 93 top / 20 bottom** (the bottom side is
  RP2040 decoupling, the CC pull-downs R13/R14, and now C54/C56/D30). The old
  "110 placements, 97/13" figures predate the rework. The ~$25 double-sided
  delta is worth it — decided 2026-07-18.
- ⬜ **Quantity: order all needed boards in ONE run.** Fixed overhead measured
  2026-07-18 (qty-5 quote): ~$148 of ~$241 is qty-independent (eng fee $25,
  setup $51, stencil ~$21, feeder fees $49). Marginal assembled board ≈$17.50.
  Two runs = paying ~$148 twice.
- ⬜ **Quantity: 20 assembled panel PCBs, one run.** Build scope is **2 fully
  assembled pads** = 2 × 9 panels + 2 spares (matches `docs/BOM.md` and
  `docs/STATUS.md`; the older "need 9 → order 10" line was single-pad and is
  retired). Extrapolating the qty-5 quote: ~$148 fixed + 20 × ~$17.50 ≈
  **$500 USD + ~$40 shipping**. Treat that as a sanity band, not a quote —
  component price breaks and the larger PCB-area line will both move it.
- ⬜ Slow build time (3–4 day assembly); expedite was +$49 for one day.
- ⬜ "Confirm Production file" / "Confirm Parts Placement" options: cheap
  ($1.50 total on the quote) — keep them, and actually respond to the DFM
  emails.

## 6. Final human pass

- ⬜ Page through JLC's gerber viewer (it renders the same artwork the fab
  uses): board outline, layer order (F/In1=GND/In2=power/B), silk name +
  "Rev. 1.0" + JLCJLCJLCJLC placeholder present, logo copper/mask pair intact.
- ⬜ Sanity-check the total against the qty-20 band above (~$500 + ~$40
  shipping). The recorded qty-5 baseline was $240.78 — if the qty-20 quote
  comes in near *that*, something is wrong with the quantity field.

## Deliberately NOT blocking the order

- Project logo (waiting on artwork; add via the verified exposed-copper flow +
  sliver check when it exists).
- THVD1429 cost (swapped from THVD1419 2026-07-19: the 1419 is the 250kbps
  grade — can't do the 1Mbps bus; 1429 = 20Mbps, drop-in, LCSC C1850236,
  and cheaper: $3.45@10+ vs $4.53. Verify JLC live stock at order time.
  SIT3485-class sub only if robustness trade is ever accepted).
- Accepted-for-rev-A review items (U5 thermal, +5V post-diode margin,
  INT-into-dead-panel, hot-plug/SI/ADC-B.Cu) — these are BRING-UP
  measurements, not order blockers.

## Post-order

- ⬜ Respond to JLC DFM/engineering emails same-day (they hold the order).
- ⬜ On arrival, before mounting in a pad: visual QA against the rotation
  list above, then bench bring-up per the accepted-items list (USB-only
  first: 3.3V rail, BOOTSEL/flash enumeration; then 12V: U5 temperature at
  real load, 5V rail margin, 12V-sense threshold, FSR ADC noise floor,
  WS2815 chain, RS-485 loopback, INT line).
