# Harness parts index — our design

Connectors, contacts, cable and tooling for the harnesses **this project
builds**. Board-side parts are also in the PCB BOMs (`docs/BOM.md`); this file
covers the cable-side halves, the wire, and everything that has to be bought
separately.

Sourcing model: **DigiKey for Teensys only** (already bought). Everything else,
including through-hole, comes from JLC/LCSC.

The **stock** pad's parts are a separate record: `stock-smx/PARTS.md`.

> ⚠ **Read `stock-smx/PARTS.md` → "Connector gender conventions" before
> ordering any housing.** JST and Molex use "receptacle" to mean *opposite*
> contact genders, and this pad contains both vendors. A full audit in 2026
> found the entire JST YL side labelled backwards. The one-line rule:
> **JST `xxR` = receptacle = male pins; JST `xxP` = plug = female sockets;
> Molex is the exact opposite.**

## Board-side (already on the PCBs, listed for mating reference)

> ⚠ **`docs/BOM.md` IS THE SOURCING AUTHORITY, NOT THIS FILE.** The LCSC column
> here went stale and read `TODO` for parts that `docs/BOM.md` had already
> sourced with part numbers, which cost a round of confusion on 2026-08-16.
> Corrected below. **If the two ever disagree again, believe `docs/BOM.md`** and
> fix this table — never the other way round.

| Ref | Part | LCSC | Used by |
|-----|------|------|---------|
| Master J1 | Micro-Fit 3.0 3-pin RA header (436500300) | C503478 | `rs485-chain.yml` |
| Master J2 | KANGNEX WJ500V-5.08-2P screw terminal | C8465 | `underglow.yml` |
| Master J3–J11 | JST B2B-XH-A, 2-pin vertical | C158012 | `int-home-run.yml` |
| Carrier J214 | KANGNEX WJ500V-5.08-2P screw terminal | C8465 | `int-home-run.yml` |
| Carrier J201/J202/J203/J206 | JST B2B-PH-K-S, 2-pin top entry | C131337 | `fsr-panel.yml` |
| Carrier J204/J207 | Micro-Fit 3.0 3-pin RA header (436500300) | C503478 | `rs485-chain.yml` |
| Carrier J205/J208 | Micro-Fit 3.0 2-pin RA header (436500200) | C192562 | `power-column.yml` |

### The carrier↔brain interface: `J210–J213` / `J301–J304`

**They are ordinary 2.54 mm pin headers and sockets**, and the footprints have
been in KiCad all along — read straight out of `dual-panel.kicad_pcb`
2026-08-16, which settles a long-running "UNSOURCED, type unknown" note:

| Ref | Board | KiCad footprint | Part | LCSC |
|-----|-------|-----------------|------|------|
| J210–J213 | carrier, B.Cu | `PinHeader_1x08_P2.54mm_Vertical` | HanElectricity **2541WV-08P** — 1×8 male, 2.54 mm, **6 mm mating pin / 3 mm tail**, gold, 3 A/pin | **C5383116** |
| J301–J304 | brain | `PinSocket_1x08_P2.54mm_Vertical` | CONNFLY **DS1023-1x8SF11** — 1×8 female socket, 2.54 mm, 8.5 mm body, gold, 3 A/pin | **C7509515** |

**SOURCED 2026-08-16**, closing a long-standing order blocker. Four of each per
panel = **32 pins per side**.

> ⚠ **QUANTITY CORRECTED 2026-08-18 — this line caused a live cart shortage.**
> It previously read *"36 of each for a 9-panel pad, 50 ordered"*, and the LCSC
> cart was built to that 50. **36 is the ONE-PAD figure.** The build scope is
> **20 panel assemblies**, so the need is **4 × 20 = 80 of each**, and 50 was
> 30 short on both lines. See the scope banner in `docs/BOM.md`.

The
6 mm pin into an 8.5 mm socket cannot bottom out, so separation is set by the
plastics meeting — which is the design intent (see the stack note below).
3 A/pin is ample against a 0.44 A panel. An earlier version of this table
called them "Micro-Fit 3.0 power/RS-485 in/out", which was simply wrong — they
carry the whole board-to-board interface and have nothing to do with either.

**⚠ The one spec that is not commodity — mating pin length.** From
`docs/DUAL_PANEL.md` (mechanical stack): board separation is set by *the two
plastics meeting*, not by pins bottoming out, and that only holds while the
mating pin is shorter than the socket is deep. So order headers with a
**6.0 mm mating pin AND a ≥3.0 mm solder tail**. Some "short" headers hit a
short overall length by trimming the *tail* instead, leaving nothing to solder
through the carrier. **Most 2.54 mm header listings state neither dimension** —
this is the thing to check on DigiKey, not pitch or pin count.

**The spacer is load-bearing and must never be omitted** — without it, tightening
pulls the brain into the carrier and these connectors absorb the entire clamping
force. With the parts above the stack is **11.04 mm**, so use **12 mm** M3
standoffs, not the 11 mm the original design assumed. See `docs/DUAL_PANEL.md`
→ "Mechanical stack".

**Not a harness part** — no cable in this directory touches them; they gate the
*PCB* order, and sourcing them closed the last item in
`docs/PRE_ORDER_CHECKLIST.md` § 0.


## Cable-side (what these harnesses need)

All sourced in `docs/BOM.md`; repeated here for mating reference.

> ### Sourcing status, updated 2026-09-01 — **one item left to buy**
>
> **100× 0.34 mm² ferrules are the only outstanding purchase in the entire
> project**, and they re-order domestically. Everything else is purchased, in
> hand, on a placed order, or deferred by an explicit decision.
>
> **The Wago 221-415 blocks are PURCHASED** (10-pack, $14.53 — not sold in
> sixes), which closed the sourcing list on 2026-08-22. The ferrules re-opened it
> on 2026-09-01: the sizes were settled at the bench, but only ~30 of the
> 0.34 mm² are on hand against 36 needed for two pads.
>
> | state | items |
> |---|---|
> | ✅ in hand | Teensy 4.0 ×2, SM 2.5 pigtail, 12 mm standoffs, heat-shrink, zip ties, M3, crimpers (SN-28B, PA-09), Molex extractor |
> | ✅ purchased | cable (50 m RVSP + 20 m 2C), fuse holders, T8AL250V cartridges, **Wago 221-415 (10-pack, $14.53)** |
> | 📦 on the LCSC order | all board parts, Micro-Fit/XH housings + crimps, XT30 |
> | 🛒 **still to buy** | **ferrules, 100× 0.34 mm²** (sizes closed 2026-09-01; ~30 on hand against 36 needed) |
> | ⏸ deferred by decision | printed carrier (dimensions off the real Wago bodies + fuse holder in hand) |
>
> **Quantities are for TWO pads throughout** — see the scope banner in
> `docs/BOM.md`. This is settled and is not to be re-derived.

| Part | For | LCSC | Qty per pad |
|------|-----|------|-------------|
| JST XHP-2 housing | INT, master end | C144401 | 9 |
| JST SXH-001T-P0.6N contact | INT, master end | C385122 | 18 |
| Molex 436450200 Micro-Fit 2-ckt receptacle housing | 12V | C114089 | 12 |
| Molex 436450300 Micro-Fit 3-ckt receptacle housing | RS-485 | C259740 | 18 |
| Molex 430300001 Micro-Fit crimp, 20–24 AWG | 12V + RS-485 | C259786 | 168 |
| **JST SMR-03V-B** housing (receptacle, 3-way) | underglow, hand-made fallback | **C157907** | 1 (10 ordered) |
| 3-pin SM 2.5 LED-strip pigtail pair, 22 AWG | underglow — the actual plan | **ON HAND** | 1 pair |
| **Wago 221-415**, 5-way lever block | 12V fan-out | ✅ **PURCHASED 2026-08-22** — 10-pack, $14.53 (genuine); not sold in sixes | 3 |
| **XT30U-F** (PSU side) | PSU→fan-out | ✅ **ON THE LCSC ORDER** — **C99102**, 10 @ $2.06 | 1 |
| **XT30U-M** (load side) | our trunk + stock reconnect | ✅ **ON THE LCSC ORDER** — **C99101**, 10 @ $2.28 | 2 |
| Inline fuse holder, 5×20 mm, **18 AWG leads** | trunk | ✅ **PURCHASED** (Amazon 5-pack, $11.99) | 1 |
| **T8AL250V** glass cartridge, 5×20 mm, time-lag | trunk | ✅ **PURCHASED** (Amazon 10-pack, $6.99) | 1 |
| Heat-shrink (colours + clear), zip ties, M3 screws | identification, strain relief, carrier | ✅ **ON HAND** | — |
| M3 female-female standoff, **12 mm** | carrier↔brain spacer | ✅ **ON HAND** — see below | 3/panel |
| **Wire ferrules** — **0.34 mm² (0.8 mm ID)** ×36 for the INT conductors, **0.5 mm² (1.0 mm ID)** ×4 for J2 | J214 INT, underglow DATA, J2 GND tie | 🛒 **SIZES CLOSED 2026-09-01; BUY 100× 0.34 mm²** — only ~30 on hand against 36 needed, and they are a single-use consumable. 0.5 mm² needs no purchase (~30 on hand, 4 needed). See below | 20/pad |
| Molex **11-03-0043** extraction tool | Micro-Fit rework | ✅ **BOUGHT** (Newark) | 1 |

### Standoffs — CLOSED 2026-08-16, nothing to buy

**12 mm M3 female-female is the right part and it is already on hand.** The
stock of 4/6/8/10/12 mm even sizes cannot make 11 mm, and that does not matter:
**11 mm was never the target once the connectors were sourced.** The connectors
stack to 11.04 mm, so an 11 mm spacer is 0.04 mm *short* and lets the plastics
take clamping load — the exact failure the spacer exists to prevent. 12 mm
leaves a 0.96 mm air gap with ~5.04 mm of pin engagement and 3.4 mm of cavity
depth still spare. See `docs/DUAL_PANEL.md` → "Mechanical stack".

**Taller is the safe direction here**; shorter is the unsafe one. Do not stack
4+6 to chase 10 mm.

⚠ **The open question is quantity, not size: 3/panel × 20 panels = 60.** Count
what is actually in the parts bin before assuming it is covered.

### Ferrules — DEFERRED to a bench measurement, 2026-08-16

Every ferrule in this build lands in a **screw terminal**. The Wago lever blocks
take **bare stranded conductor and want no ferrule at all** — the 221 series is
designed for it.

| Where | Conductor | Qty/pad | Qty, 2 pads |
|---|---|---|---|
| carrier **J214** INT (2 conductors × 9 panels) | 22 AWG | 18 | 36 |
| master **J2 pin 1**, underglow DATA from the SM pigtail | 22 AWG | 1 | 2 |
| master **J2 pin 2**, GND tie to the Wago fan-out | 20 AWG | 1 | 2 |

#### ⚠ Do not pick the size from the AWG label — two conventions disagree

**This bit a sizing call on 2026-08-16 and the first answer was wrong.**

| source | 0.5 mm² is… | 0.75 mm² is… |
|---|---|---|
| Interpower chart, DIN 46228-4 colours | **20 AWG** (white) | **18 AWG** (gray) |
| Multicomp `E0508` via Farnell/Newark | **22 AWG** | — |
| most AliExpress listings | **22 AWG** | **20 AWG** |

Both are defensible and neither is a typo. The DIN column maps by **conductor
cross-section** (22 AWG = 0.326 mm², so strictly it "is" a 0.34); the vendor
labels map by **what actually crimps well**, which is the more useful number.

**Barrel inner diameter (`D1`) is what decides it, and CSA is a poor proxy:**

| ferrule | `D1` barrel ID |
|---|---|
| 0.5 mm² | **1.0 mm** |
| 0.75 mm² | **1.2 mm** |
| 1.0 mm² | 1.4 mm |

A **22 AWG stranded** conductor's bare bundle measures roughly **0.76–0.85 mm**
(7/30 ≈ 0.765, 19/34 ≈ 0.80), so it seats in the 0.5 mm² barrel with room to
crimp. A 0.34 mm² ferrule's barrel is ~0.8 mm — the wire would barely enter.
**Our cable makes this worse, not better: RVSP's R is 软 = fine-stranded, and
fine-stranded runs a wider bundle than standard stranding at the same CSA.**

So the working assumption is **0.5 mm² for the 22 AWG conductors and 0.75 mm²
for the 20 AWG GND tie** — i.e. the vendor labelling, not the DIN CSA column.

#### The measurement that closes this

**Not orderable yet, and deliberately left open.** Strip ~20 mm of the actual
22 AWG RVSP when the reel arrives, measure the bare bundle OD with calipers, and
pick the ferrule whose `D1` clears it by ~0.1–0.2 mm. An assortment is already on
hand, so buy only what the bin turns out to lack.

Ferrules are also the one item here that re-orders **domestically without a
month's wait**, which is why they are the right thing to defer rather than guess.

#### Reel landed 2026-08-31 — measured ~0.6 mm, and that is SMALLER than expected

The bare bundle of a stripped 22 AWG RVSP conductor reads **~0.6 mm**, below the
0.76–0.85 mm predicted above, and fine stranding should have pushed it *up*. So
the reel's copper is plausibly **24 AWG sold as 22** — common on AliExpress cable
and, here, almost entirely harmless: the insulation OD passes both connector
windows (`docs/BOM.md`), the Molex `430300001` crimp covers 20–24 AWG, the JST
`SXH-001T-P0.6N` covers 22–28 AWG, and the conductor carries microamps.
**The ferrule barrel is the only thing the answer changes.**

Two caveats before acting on 0.6 mm: calipers **compress** a fine-stranded
bundle, so the figure is a lower bound; and fill matters more than clearance —
a 0.5 mm² barrel (`D1` 1.0 mm, area 0.79 mm²) is only ~36% full at 0.6 mm,
against ~56% for a 0.34 mm² barrel (`D1` 0.8 mm).

#### ✅ CLOSED 2026-08-31 — **16/0.13 mm COPPER, 0.212 mm² = 24 AWG**, sold as 0.3 mm² / 22 AWG

Copper is **confirmed by measurement, not assumed** — see "Is it CCA?" below.
The cable is one standard construction below its marking: `16/0.13` (nominal
0.2 mm²) jacket-printed as `2x0.3mm²`.

End-to-end resistance of one conductor across the nominally 50 m reel measured
**4.5 Ω**. That is **90 Ω/km**, i.e. a CSA of **0.19 mm²**:

| | Ω/km | expected over 50 m | CSA |
|---|---|---|---|
| 22 AWG (labelled) | 52.9 | 2.65 Ω | 0.326 mm² |
| **24 AWG (measured)** | 84.2 | **4.21 Ω** | **0.205 mm²** |
| reading | **90** | **4.5 Ω** | **0.19 mm²** |

**Corrected: the meter's own leads read 0.3 Ω**, so the conductor is **4.2 Ω =
84.0 Ω/km**, which is 24 AWG copper to within 0.1%. Either way it is ~60% above
what 22 AWG would read, far outside any measurement slop. **It also
independently confirms the caliper**: a 0.6 mm bare bundle at a typical ~75%
stranding fill is ~0.21 mm², the same answer from a completely different
measurement. Two methods agreeing closes this.

*(Read as a shorted loop instead, 4.5 Ω would imply 0.38 mm² — thicker than the
label — which flatly contradicts the 0.6 mm bundle. The single-conductor reading
is the self-consistent one.)*

**Consequences: ferrule size, and nothing else.**

- ✅ **Insulation OD** (1.45 mm) passes both connector windows regardless — that
  check never depended on the copper.
- ✅ **JST XH `SXH-001T-P0.6N`** covers 22–28 AWG: 24 sits mid-range, a better
  fit than 22 was.
- ⚠ **Molex `430300001`** covers 20–24 AWG: 24 is at the **bottom edge** of the
  range. Still in spec, but crimp edges deserve verification — **pull-test the
  first few Micro-Fit crimps** before committing to the remaining 200.
- ✅ **Electrically irrelevant.** INT sinks ~0.33 mA through the master's 10 k
  pull-up; RS-485 drives ~40 mA into the doubly-terminated pair, where 8.4 m of
  24 AWG loop adds ~0.7 Ω against a 54 Ω load — about 1% of the differential
  swing. Gauge on this cable was always mechanical, never current.

**Ferrule: 0.34 mm² (`D1` 0.8 mm) is now the pick**, not the 0.5 mm² working
assumption above — a 0.6 mm bundle fills the 0.8 mm barrel ~56% versus ~36% in a
1.0 mm barrel. Two caveats:

1. **Assortment boxes often start at 0.5 mm².** If the bin has no 0.34, that is
   the domestic re-order — the reason this item was deferred in the first place.
2. **Or fold the conductor back on itself** and crimp the doubled bundle into
   the 0.5 mm² ferrule already on hand. Standard practice for an undersized
   conductor, and it needs no purchase at all.

Either way, **crimp one and tug-test it** before doing all 38.

#### ✅ CLOSED 2026-09-01 — 0.34 mm² confirmed at the bench; **the tool was the problem, not the barrel**

**Ferrule: 0.34 mm² (1.25 mm OD / 0.8 mm ID). Crimped and pull-tested on the
real RVSP conductor. Neither caveat above was needed — no domestic re-order, and
no fold-back.**

The first attempts pulled out of the barrel under a firm tug, which reads like a
fill problem and is not one. The crimper is an **HSC8 6-4** — a self-adjusting
square four-indent tool, **0.25–6.0 mm² / AWG 23–10**. It has *no die stations*:
one jaw set spans the whole range and an internal stop decides closure. A
0.34 mm² ferrule therefore sits at the very bottom of a 24:1 span, which is
exactly where these tools run out of travel and deliver a well-shaped,
under-compressed crimp.

**Tightening the tool's adjustment pin fixed it outright.** At the new setting
the conductor **snaps before the ferrule will come off** — failure outside the
barrel, i.e. the crimp is stronger than the wire, which is the pass criterion.

⚠ **Two consequences of the adjustment:**

1. ✅ **The global stop was checked and is fine.** Tightening for 0.34 mm² also
   tightens the **0.5 mm² GND-tie ferrules** (master J2 pin 2, 20 AWG, 2 per
   pad — the only large ferrules in the build). One was crimped at the new
   setting and is good: no crushed or split barrel. **Both ends of the range in
   use here work at one setting**, so no re-adjustment between sizes.
2. **The tool is now off its factory setting and nothing about that is visible
   from outside.** Mark it, or the second pad rediscovers this from scratch.

**Do not conclude the 0.5 mm² ferrule or the fold-back are needed** — both were
contingency plans for a fill problem that turned out to be a tool-closure
problem. 56% fill is sufficient given an adequately compressed crimp.

#### What to buy — 0.34 mm² only, 2026-09-01

Only two screw terminals exist in the build (carrier **J214** ×9/pad, master
**J2** ×1/pad), so the demand is fully bounded. **Quantities are for two pads.**

| ferrule | where | /pad | **2 pads** | on hand | action |
|---|---|---|---|---|---|
| **0.34 mm²** | J214 INT — 9 panels × 2 conductors | 18 | **36** | ~30 | 🛒 **buy 100** |
| **0.5 mm²** | J2 pin 2, GND tie (20 AWG) | 1 | 2 | ~30 | ✅ none |
| **0.5 mm²** | J2 pin 1, underglow DATA (22 AWG) — see warning below | 1 | 2 | ↑ | ✅ none |

**Buy 100 rather than the ~6 shortfall.** It is the standard bag size and costs a
few dollars, and ferrules are **single-use** — every tool trial and every
re-termination during bring-up consumes one. 100 is ~2.8× coverage. Do **not**
buy 0.5 mm²: 30 on hand against a need of 4.

**Two things to match, not guess:**

1. **Barrel length.** 0.34 mm² ships in 6 mm and 8 mm barrels (the `E0306` /
   `E0308` convention — CSA code, then length). **Measure the barrel on one
   already in the bin and order the same**, so strip length and the tool setting
   carry over unchanged.
2. **Bare, not insulated.** The ones in use are bare (1.25 mm OD); an insulated
   0.34 mm² carries a ~3 mm plastic collar and a different strip length.

Order **domestically** — that ferrules re-order without a month's wait was the
original reason this item was safe to defer.

#### ⚠ The underglow DATA lead is 0.5 mm², NOT 0.34 — the 24 AWG finding does not travel

The "✅ USER-CONFIRMED 2026-08-16" block below says **DATA → 0.34 mm² ferrule →
master J2 pin 1**. **That is believed wrong and is superseded here.**

That conductor is **not** the RVSP reel — it is the 22 AWG extension spliced onto
the SM pigtail, i.e. *genuine* 22 AWG at 0.326 mm², bundle ~0.75–0.85 mm. The
0.8 mm barrel of a 0.34 mm² ferrule would barely accept it, exactly as the
sizing section above warns. **The 24 AWG measurement applies only to the RVSP
cable; it does not generalise to other hookup wire in the build.**

⚠ **Test-fit one before committing** — this is reasoned from the wire's nominal
gauge, not from a caliper reading of the actual on-hand wire. It is 2 ferrules
across both pads and both sizes are in the bin, so it blocks nothing.

#### Both size guesses came out ONE SIZE TOO BIG — measure the ferrule, don't trust the bin label

Measured with calipers on the two ferrules actually used, 2026-09-01:

| conductor | ferrule OD / ID | ferrule size | earlier assumption |
|---|---|---|---|
| 24 AWG RVSP (INT, underglow DATA) | **1.25 / 0.8 mm** | **0.34 mm²** | 0.5 mm² ✗ |
| 20 AWG (J2 GND tie to the Wago fan-out) | **1.45 / 1.05 mm** | **0.5 mm²** | 0.75 mm² ✗ |

Barrel ID is the identifying dimension (`D1` = 0.8 / 1.0 / 1.2 / 1.4 mm for
0.34 / 0.5 / 0.75 / 1.0 mm²); bare OD runs ~0.4 mm above it at these sizes.

**The GND tie resolves in favour of the DIN CSA column, not the vendor
labelling** — the opposite of what the "do not pick the size from the AWG label"
section above assumed. 20 AWG is 0.518 mm², so the 0.5 mm² ferrule is an almost
exact CSA match, and a ~0.95 mm bundle in a 1.0 mm barrel fills it ~80% — better
than the 56% on the INT conductors. **Neither convention wins in general; the
lesson is that only the caliper settles it.**

**Pull-test the assembled joint, not the bare crimp** — wire → ferrule → seated
in the WJ500V with the screw torqued. The screw deforms the ferrule and does
real retention work, so that is the joint that actually ships.

#### The jacket marking explains where the "22 AWG" came from — and indicts the cable, not the listing

Printed on the jacket: **`ZC-RVSP 2x0.3mm² 300/300V JB/T8734.5-2016 PVC CABLE`**.

- **`2x0.3mm²`** — the cable is sold by **metric CSA, not AWG**. 0.3 mm² is
  nearest to **22 AWG** (0.326 mm²), so the seller's "22 AWG" was an honest
  conversion of the printed marking. **The shortfall is at the factory, not in
  the listing** — there is no point disputing it with the seller.
- **It fails its own printed standard.** Flexible class-5 copper's maximum
  resistance scales as ~19.5/CSA Ω·km, giving **~65 Ω/km for 0.3 mm²**
  (≈3.26 Ω over 50 m). The reel measures **90 Ω/km / 4.5 Ω** — **38% over the
  limit**. At 0.19 mm² it is really **0.2 mm², one standard size down**, whose
  ~98 Ω/km limit it passes comfortably. 0.2 mm² ≈ **24 AWG**, which is exactly
  what the resistance and the calipers both said.
- **The conclusion is robust against reel-length error.** The reel would have to
  be **69 m** — 38% over its nominal 50 — for 4.5 Ω to read as a true 0.3 mm².
  Reels run short, not long.
- **`ZC`** = 阻燃 C, flame-retardant class C. A small unlooked-for bonus for
  cable running inside a chassis.
- **`300/300V`** — 300 V core-to-core and to earth. Against our 12 V, irrelevant.
- ⚠ **`JB/T 8734.5-2016` implies a 70 °C PVC rating**, and `docs/BOM.md` →
  "Wire specification" asks for **≥80 °C**. **Accepted deviation, not a
  problem**: these conductors carry microamps to milliamps with no self-heating,
  and the pad is a room-temperature environment. Recorded so the next reader
  does not have to re-derive it.

⚠ **For rev 2: specify the insulation OD, not the gauge — in any unit.** The
listing's "22 AWG" was wrong by a full gauge, but so was the jacket's own
`0.3mm²`, so tightening the order to metric buys nothing. A re-order from this
seller ships this cable. **Do not chase the gauge by ordering 0.5 mm² instead** —
24 AWG is mechanically and electrically fine here (see above), and a genuine
0.5 mm² conductor's insulation could breach the **1.85 mm Micro-Fit ceiling**,
turning a non-problem into a real one. Hold the vendor to **1.30–1.85 mm
insulation OD**, which is the dimension that actually gates the connectors, and
re-run the 4.5 Ω check on arrival.

⚠ **Match the printed mm², never the colour.** Two incompatible colour systems
are in circulation (DIN 46228-4 vs the French convention) and assortment boxes
mix them, so the same colour means different sizes across two boxes.

⚠ **Never tin a conductor as a ferrule substitute.** Solder cold-flows under the
screw's clamping force and the joint loosens over months. Bare fine-stranded
directly in the clamp is the legitimate no-ferrule option; tinned is not.

### Underglow — we DO build the SM 3P side

- **`SMR-03V-B` housing (receptacle, 3-way) — LCSC `C157907`**, 10 ordered
- **Contacts: BUY A PRE-MADE LED-STRIP PIGTAIL PAIR INSTEAD.** See below.

**⚠ Do not order `SMM-003T-P0.5` (`C385123`) — it is rated 28–30 AWG**, far too
fine for either the stock 18 AWG conductors or our 22 AWG pigtails. It would not
crimp our wire even when in stock. `C22362649` (DLL ZH-RT) is not a substitute
either: **ZH series, 1.5 mm pitch, 26–32 AWG** — wrong family, pitch and gauge.

**That gauge mismatch is informative.** Genuine JST SM tops out near 22 AWG, so
the stock pad's **18 AWG** conductors in an SM 3P mean **the strips' connector is
an LED-strip "SM 2.5" clone**, not a true JST part — the ubiquitous
WS2811/WS2812 strip connector, same shell, built for 20–22 AWG.

**So use a pre-made 3-pin SM 2.5 LED-strip pigtail pair** (**a 22 AWG pair is
already on hand**). It matches what is actually on the strips, arrives already
crimped — no SM contacts and no SM crimp tool — and its **bare tails land
straight into the Wago lever block (12 V, GND) and the J2 screw terminal
(DATA)**, so there is nothing to crimp at either end.

**✅ USER-CONFIRMED 2026-08-16, and this is the whole build:** splice 12 V, GND
and DATA onto the on-hand SM 3P pigtail, then **12 V → Wago block, GND → Wago
block, DATA → 0.34 mm² ferrule → master J2 pin 1**. **No `YLP-01V`/`YLR-01V` is
involved** — that idea belonged to the superseded keep-the-stock-harness plan.
The `SMR-03V-B` (`C157907`) stays a hand-made fallback only.

⚠ **Expect to splice, not just terminate.** LED-strip pigtails ship with roughly
**15 cm** of tail, which will not reach from the strip connector to the fan-out
and the master. Extend with 22 AWG (12 V/GND) and 22 AWG (DATA), solder-and-
heatshrink or a lever block — this is the one deliberate splice in the build,
and it is why the run is listed as "3-conductor 22 AWG, underglow adapter" in
the cable table.

⚠ **Get the pinout onto the spliced side before energising: pin 1 = GND,
pin 2 = DATA, pin 3 = 12 V.** Reversed, 12 V lands on DATA and the first LED
dies. Label the tails as you splice; do not rely on the pigtail's wire colours,
which LED-strip vendors do not standardise.

**22 AWG is ample here:** underglow draws 2.44 A at full white across all 44
groups, against ~7 A of chassis ampacity for 22 AWG and ~40 mV of drop. Stock's
18 AWG was oversized.

⚠ **Test-fit for gender; do not trust the listing.** LED-strip vendors label
"male/female" by housing rather than by JST's contact convention — the exact
trap documented in `stock-smx/PARTS.md`. Buy the pair, use whichever half mates
the strips' own pigtail, keep the other as a spare.

**Pinout — getting this wrong destroys the first LED: pin 1 = GND, pin 2 = DATA,
pin 3 = 12 V.** Confirmed at the 2026-08-08 teardown, and it is the *reverse* of
what was originally assumed. 12 V and GND come from the Wago fan-out, DATA from
master J2 pin 1.

**Gender:** `SMR` is the receptacle, so it holds **male pin** contacts = `SMM`
(SM-Male). The strips' own moulded pigtail is the `SMP-03V-BC` plug with female
sockets. Consistent with the convention table in `stock-smx/PARTS.md`.

> ⚠ **A note in `docs/UNDERGLOW.md` says this harness "may not be needed at
> all" — that is CONDITIONAL and does not apply.** It assumed we *keep* the
> stock harness segment that feeds the SM 3P, in which case the master would
> only drive one wire into the stock `YLR-01V`. **We remove all SMX wiring**, so
> that segment and its `SMR-03V-B` go with it. What survives is only the strips'
> moulded `SMP-03V-BC`, which we must mate. No `YLP-01V` is required.

### 12V fan-out — Wago 221-415 lever blocks

**Three blocks per pad**, and the count is settled rather than provisional:

- **+12 V rail: ONE block.** Exactly 5 ports — trunk in, three column feeds,
  underglow. It lands at 5 only because the external cabinet barrel input is
  deferred and not designed in; adding it later forces a second jumpered block.
- **GND rail: TWO blocks, jumpered.** Six ports — the same five plus the
  **master GND tie**, which gets its own port and its own lead so that
  unplugging underglow cannot break the RS-485/INT ground reference.

Rated **32 A / 12 AWG**, so the lever is never a limiting element in this
harness, and it doubles as the tool-free disconnect for the whole 12 V system —
which is why no connector is needed between the PSU and the fan-out beyond the
XT30. Accepts 24–12 AWG, covering our 20 AWG trunk and columns and the 22 AWG
underglow tails.

Mounted on the Daygreen converter's own two M3 holes — **57 mm centres, 3 mm
holes, horizontal** (measured 2026-08-16). Full harness: `12v-trunk.yml`.

#### ✅ Is it CCA? — NO, it is copper. Settled 2026-08-31 by resistance × mass

**`docs/BOM.md` → "Wire specification" says "never CCA", so this was a
spec-compliance question, not a curiosity. The reel passes; no re-buy needed.**

**Keep the method.** Multiply the two measurements and the cross-section
cancels, leaving a pure material fingerprint: `R/L = ρ/A` and `m/L = A·d`, so
**`(R/L)·(m/L) = ρ·d`** — independent of CSA, of stranding fill, and of caliper
technique, i.e. independent of everything that made the individual measurements
ambiguous for three rounds.

Measured: **84.0 Ω/km × 1.90 g/m = 159.6**, from a 20 cm stripped conductor
weighing **0.38 g**.

| | ρ·d | vs measured |
|---|---|---|
| **copper** | **154.5** | **+3%** |
| CCA-15% | 93.8 | +70% |
| CCA-10% | 88.4 | +81% |
| pure aluminium | 76.3 | +109% |

Copper by better than a factor of two over the nearest alternative.

With the metal fixed, CSA follows from resistance alone: **0.212 mm²**, exactly
the standard `16/0.13` build (16 strands confirmed by count). Predicted
81.3 Ω/km against 84.0 measured — a reel 1.5 m short of nominal closes that.

**What each signal was actually saying:**

| signal | reading | verdict |
|---|---|---|
| bend/fatigue | strands would not break | ✅ correct — copper |
| cut faces | coppery | ✅ correct, but would look identical on CCA |
| strand diameter | 100 µm (true 130) | calipers compress a soft strand ~30 µm |
| **stripped bundle Ø** | 0.60, then ≥0.73 mm | ❌ **the one misleading measurement** |

⚠ **Never infer CSA from stripped-bundle diameter on fine-stranded cable.**
Bunched strands splay once the insulation is off, so the relaxed bundle
overstates the conductor — here ~0.73 mm against ~0.62 mm compacted, which is
what made CCA look like the leading hypothesis twice. Use strand count × strand
diameter, mass, or resistance.

The superseded reasoning is kept below for the arithmetic.

CCA's resistivity is ~1.5× copper's, which means **a full-size 0.3 mm² CCA
conductor and a shrunken 0.2 mm² copper one read almost identically.** The
resistance measurement is degenerate between them:

| candidate | Ω/km | over 50 m | bundle Ø | **g/m** |
|---|---|---|---|---|
| copper, 0.205 mm² (24 AWG) | 84.1 | **4.21 Ω** | ~0.60 mm | **1.84** |
| copper, 0.300 mm² (as marked) | 57.5 | 2.87 Ω | ~0.73 mm | 2.69 |
| CCA-15%, 0.300 mm² (as marked) | 86.0 | **4.30 Ω** | ~0.73 mm | **1.09** |
| CCA-10%, 0.300 mm² (as marked) | 88.6 | **4.43 Ω** | ~0.73 mm | 1.00 |
| **measured (lead-corrected)** | **84.0** | **4.20 Ω** | ~0.60 mm | — |

Copper-24 AWG fits to 0.1% and CCA is 2–5% off, but that gap is **inside
reel-length slop** — a reel 3% short of its nominal 50 m swings it. The 0.60 mm
caliper reading favours copper, but calipers *compress* a fine-stranded bundle,
so a true 0.73 mm bundle squeezing to 0.60 is not impossible either. **Neither
measurement discriminates. Do a physical test.**

1. **Cut a single strand at a shallow angle and look at the cut face** under
   magnification. CCA is silver-white aluminium under a thin copper skin, and it
   is unmistakable on a fresh cut. Free, instant, decisive.
2. **Weigh a stripped 2 m sample** if the cut is ambiguous — density differs by
   ~2.5×, dwarfing every error above. There are ~23 m spare.

   | conductor | 2 m stripped |
   |---|---|
   | copper, 0.205 mm² | **3.7 g** |
   | copper, 0.300 mm² | **5.4 g** |
   | CCA-18%, 0.300 mm² | **2.3 g** |

#### ❌ SUPERSEDED — "the bundle is ≥0.73 mm, so CCA leads"

*Wrong, and kept only to record why. The stripped bundle splays; see the splay
warning above.*

The 0.60 mm figure above was a **compressed** reading and is withdrawn. Set to
0.73 mm, the calipers will not pass the bundle without squeezing it — so the
free diameter is **at least** 0.73 mm, i.e. the conductor is close to its marked
0.3 mm² after all. **Combined with 84 Ω/km, that rules pure copper out across
every plausible stranding fill:**

| stranding fill | implied CSA | implied ρ | vs copper (17.24) | implied Cu by volume |
|---|---|---|---|---|
| 0.65 (loose bunched) | 0.272 mm² | 22.9 | **+33%** | ~37% |
| 0.72 (typical) | 0.301 mm² | 25.3 | **+47%** | ~18% |
| 0.78 (tight concentric) | 0.326 mm² | 27.4 | **+59%** | ~5% |

Even the most conservative corner is a third too resistive to be copper, and the
typical one lands at **~18% copper by volume — squarely in the CCA-10A/15A
range.** Tinning accounts for a few percent, not thirty.

**So the picture inverts: the factory did not shave the cross-section, it
substituted the metal.** `0.3mm²` on the jacket is dimensionally honest; the
misrepresentation is `JB/T 8734.5-2016`, a **copper-conductor** standard, and by
extension the `RVSP` type code, whose R/V/S/P say nothing about the metal but
whose whole family is specified in copper.

**This is still a two-hypothesis situation until a physical test is run** — the
≥0.73 mm is a feel-based go/no-go against knife-edge jaws, not a measurement.
**Run the cut-face check before acting on it.** But it is now the hypothesis to
disprove rather than the long shot.

**If it is CCA, seriously consider re-buying rather than accepting.** The
electrical penalty is still nil at these currents — the problem is mechanical
and it lands on the **204 crimps**: aluminium creeps under crimp pressure and
grows an insulating oxide, so joints relax over months, and aluminium
work-hardens and cracks where copper merely bends. In a pad that gets stomped on
and whose panels get lifted for service, that is the wrong failure mode to
design in, and re-terminating later costs far more than the cable does. Soldered
joints (the braid-to-drain lead) are unaffected — those only touch the copper
cladding.


#### Mounting: printed PETG carrier — DECIDED 2026-08-16, modelled later

**The original plan stands: a printed carrier on the Daygreen's own two M3
holes.** Wago's off-the-shelf `221-500` carrier was evaluated and **rejected in
favour of printing** — it fits the `221-415` and does DIN-35 rail *or* screw
mount, so it would have removed the CAD work, but its envelope was never
verified (WAGO's datasheet 403'd, and the 77.6 mm length found in search looks
wrong for a 30 mm connector) and it buys nothing that printing does not.

**Deliberately sequenced after the parts arrive.** Model the carrier with the
real Wago bodies and the real fuse holder in hand rather than from datasheet
nominals — the pockets have to clear the levers through their full swing, and
that is a measurement, not a specification. Nothing else waits on it.

Design notes for when it happens:

- **PETG, not PLA** — this shares a compartment with the supply.
- Reuses the Daygreen's **two M3 holes, 57 mm centres, 3 mm dia, horizontal**
  (measured 2026-08-16), so no new drilling and the position needs no decision.
- Pockets sized for the levers to **swing fully open** while installed.
- **Zip-tie strain relief ahead of each block.**
- Engraved `+12V` / `GND` — the two rails are otherwise identical orange blocks,
  and getting them backwards puts 12 V on the ground network.
- **Consider housing the fuse holder in the same part**, which makes the fuse
  serviceable without unpacking the compartment.
- Wago's own `221-500` remains a useful dimensional reference for the pocket.

**The only thing still to buy is 100× 0.34 mm² ferrules** — the Wago blocks,
XT30 pairs and fuse holder + T8A cartridge are all purchased. Standoffs and the
extraction tool are settled. The old "fork/spade lugs, PSU stud size" row is
**deleted**: there is no PSU stud, and the fan-out replaced the whole idea.

### ⚠ Fuse holder — the pigtail gauge is the trap, not the fuse

The cheap inline 5×20 mm screw-type holders that dominate search results
(uxcell, PNGKNYOCN and similar) ship with **22 AWG pre-attached leads**. Our
trunk is 20 AWG carrying **6.34 A worst case**, so a 22 AWG pigtail would become
the thinnest conductor in the entire 12 V path — in series with everything, bundled
inside a warm compartment. **The fuse would be protecting wire thinner than the
wire it protects.**

**✅ RESOLVED 2026-08-16 — an inline holder with 18 AWG leads was selected**
(Amazon 5-pack, $11.99; ships with 1 A fast-blow cartridges that are discarded).
The alternative was a panel-mount holder with solder tabs wired in our own
20 AWG, which is still the better answer *if* the printed carrier ends up
housing the fuse — it would make the fuse serviceable without unpacking the
compartment.

⚠ **Still verify the holder's own current rating: want ≥10 A.** Many 5×20 mm
holders are specified around 6 A, which would make the *holder* the weak link
rather than the fuse. **Lead gauge and holder rating are independent specs** —
18 AWG leads make a low rating unlikely but do not prove it.

#### The cartridge — `T8AL250V`, selected 2026-08-16

Amazon 10-pack, $6.99. Glass, time-lag, 5×20 mm. The marking decodes as:

| mark | meaning | verdict |
|---|---|---|
| **T** | time-lag per **IEC 60127** | ✅ the required standard — see the F1 note in `12v-trunk.yml` |
| **8A** | rated current | ✅ 79% loaded at the 6.34 A datasheet worst case |
| **L** | **low** breaking capacity | ✅ fine here, see below |
| **250V** | max voltage | ✅ a maximum; 12 V DC is trivially inside it |

**Low breaking capacity is not a compromise in this application.** Breaking
capacity is the largest fault current the fuse can interrupt without rupturing,
and low-BC is ≥35 A. Our source is a **12 V 8.5 A SMPS whose own OCP folds back**
— it cannot present anything close to 35 A. High-BC ceramic exists for mains
circuits backed by a utility transformer, which this is not.

**Glass is the better choice here, not merely acceptable:** a blown glass fuse is
visible. This pad has **no current sensing anywhere** (the master is USB-powered
and deliberately outside the 12 V path), so a fuse you can inspect by eye is the
only direct read you get on a fault.

⚠ **Do not substitute a US/UL "slow blow" 8 A.** The standards define rated
current differently — a UL 8 A part is good for only ~6 A continuous, *below*
our 6.34 A worst case, and would nuisance-blow on the load it exists to pass.
The `T` prefix is what confirms IEC.


## Crimp and extraction tooling

**RESOLVED 2026-08-16 — the tools on hand cover every crimp in the build.** No
crimper needs buying; only an extraction tool.

| Tool (already owned) | Use for | Count | Why it fits |
|---|---|---|---|
| **IWISS SN-28B** | Micro-Fit 3.0 crimps `C259786` | 168 | Terminal 430300001 is 20–24 AWG = **0.2–0.5 mm²**, mid-range for SN-28B's 0.1–1.0 mm². Major SN-28B listings advertise Molex 43030-family compatibility |
| **Engineer PA-09** | JST XH crimps `SXH-001T-P0.6N` | 18 | 32–20 AWG, dies 1.0/1.4/1.6/1.9 mm — built for exactly this narrow-pitch class |

⚠ **Pull-test the SN-28B on Micro-Fit before committing to 168 crimps.** Its die
profile targets Dupont/XH barrels, and Micro-Fit's is wider — the wire range
fits and vendors claim support, but that is *plausible*, not certified. Crimp
two or three, seat them in a housing, and tug. A terminal that seats but pulls
free under light tension is the failure that surfaces during reassembly.

**Terminal spec worth knowing** (Molex 430300001): 20–24 AWG / 0.2–0.5 mm²,
wire insulation **1.85 mm OD max**, rated **8.5 A** (7.0 A max per contact) —
against the conservative 5 A/pin this project assumes and the 1.3 A a column
actually carries. Margin is not a concern anywhere on this connector.

**Do NOT buy Molex 63819-0000.** It is the official Micro-Fit hand crimper and
comparable Molex hand tools list at **$358–379 USD** (~$600 CAD). It is a
production tool; the economics need thousands of crimps.

### The one tool to buy: `Molex 11-03-0043`, Micro-Fit 3.0 extraction

**✅ PURCHASED 2026-08-16 from Newark.**

Molex `11-03-0043` (Newark `94B5648`; DigiKey lists it as `WM9937-ND`).
Correct for terminals 43030/43031 in housings 43025/43020 — exactly our
`C259786` crimps in `C114089`/`C259740` housings. With 168 crimps going in some
will go in wrong, and the terminal locks with a plastic lance that cannot be
released with a pick without wrecking the lance, the terminal, or both.

A third-party clone is functionally fine — it is a thin-walled tube that slides
over the terminal to depress the lance, with no precision dies to get subtly
wrong.

**Correction 2026-08-16: "universal fit" is NOT a red flag.** An earlier note
here claimed it was disqualifying. Wrong — the *genuine* 11-03-0043 is itself
specified for **43030, 43031, 44372, 46235, 50011, 50080, 50200, 50058**, i.e.
Micro-Fit 3.0, Micro-Fit Plus, PicoBlade, Micro-Latch and DuraClik, 32–18 AWG.
Covering several Molex families is normal for this tool, so a listing claiming
it is plausible rather than suspect.

What *is* worth checking on a marketplace listing: that it names actual terminal
series rather than only "MOLEX connectors", and that the copy is about terminal
extraction rather than generic soldering filler. Rejected on those grounds
2026-08-16: an unbranded Amazon.ca listing with no series numbers, soldering
boilerplate, and contradictory material claims.

**Sourcing (Canada), cheapest first-ish:** it is **not on LCSC**. Mouser tends
to have a lower free-shipping threshold to Canada than DigiKey's $100; Newark
serves Canada with the same part (`94B5648`); Farnell/element14 and CPC stock it
too. Otherwise just add it to the next DigiKey order rather than paying ~$15
shipping to buy it alone.

Improvised fallback, for rescuing one or two terminals only: a short length of
thin-wall brass tube, or a cut-down blunt hypodermic needle of the right bore.
Not something to rely on across 168 crimps.

**⚠ One-digit trap: it is `11-03-0043`, not `11-03-0044`.** The `-0044` is the
**Mini-Fit Jr.** extractor — a different, larger family. This pad already
contains both families (stock uses Mini-Fit Jr. 39014041 on the 5 V columns),
so ordering the wrong one is easy and the parts look similar.


## Master USB to the PC — SOLVED BY REUSE, no parts needed (2026-08-16)

The pad already has the whole path: a **USB-C receptacle mounted on the pad
shell**, wired internally to a **USB Micro-B plug** that went to the Arduino
Micro. **Teensy 4.0/4.1 also use Micro-B**, so the stock internal cable plugs
straight into the replacement master. Nothing to buy, nothing to route, and the
pad keeps its existing external USB-C port.

Two things to verify on the bench rather than assume:

- **It is a data cable** — it must be, since it carried the stock MCU's USB, but
  confirm before blaming firmware for an enumeration failure.
- **It is good enough for USB High Speed.** The Teensy 4.x negotiates USB 2.0 HS
  (480 Mbps) automatically, which is what the 2000 Hz+ polling goal rests on. A
  cheap internal cable is a plausible thing to limit that. Short run, so it will
  probably be fine — but if HS negotiation fails, suspect this cable early.


### XT30 — the PSU-to-fan-out connector (our harness, not stock)

Replaces the stock 5.5 × 2.5 barrel on the PSU's captive cable. Chosen for
**reversibility** (see below) — the current rating was never the deciding factor.

**XT30, not XT60 — picked on wire fit.** XT30's solder cups take **16–20 AWG,
18 AWG recommended**, which is exactly our wire; XT60's are sized for 12–14 AWG,
where 18 AWG sits loose and must be made up with solder. Current is a non-issue
either way: XT30's conservative published rating is **15 A continuous** (many
sources say 30 A) against a 6.34 A worst case and a supply that can only source
8.5 A.

- **Gender: the source takes the FEMALE half.** Its contacts are recessed and
  separated by a wall, so a live supply can't be shorted by anything laid
  across it; the male's pins are exposed and belong on the load side. This is
  an ordinary source-is-shrouded safety convention and is **unrelated to the
  JST receptacle/plug naming trap in `stock-smx/PARTS.md`** — that one is about what a housing is
  *called*, this is about which side it *goes on*.
- **✅ Convention verified 2026-08-17.** XT30's **male carries the exposed metal
  pins; the female carries recessed sockets** — so female-on-the-source puts the
  only permanently-live contacts behind a shroud, and our male pins are dead
  whenever they are unplugged. Note the RC world runs *both* conventions and
  often does the opposite (male on the battery, so a battery's terminals sit
  inside a shell). That reasoning is about a source that **cannot be switched
  off**; our PSU is only live when it is plugged in, so the shrouded-source rule
  is the right one here.
- ⚠ **Confirm by looking at the parts, not the label.** Check which half has
  recessed sockets and put *that* on the PSU. Vendors do occasionally name XT
  halves by housing rather than contact — the same class of error as the JST
  naming trap — and 10 of each were ordered, so either label being wrong costs
  nothing.
- **Parts on order: `XT30U-F` LCSC `C99102` and `XT30U-M` LCSC `C99101`**, 10 of
  each against a need of 2F + 4M for two pads. The `U` variant has the longer
  insulated housing.
- **Need per pad: 1 female (PSU) + 2 male** — one for our trunk cable, one for
  the cut stock 20 AWG tail, which is what keeps the modification reversible.
  Leaves a spare female.
- **Solder cup, no crimp tool** — the only connector in the pad that doesn't
  need one. Heatshrink each contact before assembling the housing.
- **Keyed** (one chamfered corner), so it cannot be mated backwards — only
  mis-wired. Convention is straight side positive, angled side negative; since
  we fit all three halves, internal consistency plus correct PSU polarity is
  what actually matters.

Wire colors for every harness live in `WIRE_COLORS.md`.


## Cable

| Type | Spec | Used by |
|------|------|---------|
| Jacketed 2-conductor | 20 AWG | 12V columns |
| **Shielded twisted pair (RVSP), 2-core** | **22 AWG** | **RS-485 *and* INT home runs — one reel, see below** |
| 3-conductor | 22 AWG | underglow adapter |

### RS-485 and INT share ONE cable — 22 AWG, decided 2026-08-16

They were spec'd separately (RS-485 "22–24 AWG", INT "24 AWG"), and
`rs485-chain.yml` had drifted to 24 AWG against `docs/BOM.md`'s 22. Both runs
are **2-core shielded twisted pair**, both carry signal-level current, and both
now use the same 22 AWG reel. Only the *shield termination* differs:

- **RS-485** lands the shield on Micro-Fit **pin 3** (hybrid grounding, DC-tied
  at the master only).
- **INT** trims and heatshrinks the shield at both ends. **Do not bond it to the
  INT GND conductor** — that is a signal return, and paralleling a shield across
  it restores the loop area the twisting exists to remove.

**Why 22 and not 24 — it is the only gauge inside both connector windows:**

| constraint | limit | 24 AWG RVSP | 22 AWG RVSP |
|---|---|---|---|
| JST XH `SXH-001T-P0.6N` insulation OD | **≥ 1.30 mm** | ~1.3–1.4 — at the floor | comfortable |
| Molex Micro-Fit `430300001` insulation OD | **≤ 1.85 mm** | fine | comfortable |
| Micro-Fit wire range | 20–24 AWG | bottom edge | mid-range |
| XH wire range | 22–26 AWG | mid | top edge, fine |

Current is microamps (INT) to milliamps (RS-485), so **gauge here is purely
mechanical** — it is chosen to crimp reliably, not to carry load.

⚠ **The one thing to verify with the seller or on arrival: conductor insulation
OD must land in 1.30–1.85 mm.** That is the *individual conductor's* insulation,
not the outer jacket. It is the only spec that can sink this cable, and it is
rarely listed — measure before crimping 204 contacts.

Cross-plugging is impossible despite the shared cable: RS-485 terminates in
3-circuit Micro-Fit, INT in JST XH and a 5.08 mm screw terminal.

### Grommets — dropped as a purchase, kept as an assembly check (2026-08-17)

A "grommets where cable crosses frame metal" line sat in `docs/BOM.md` from the
start with no quantity and no candidate. **It was precautionary, not derived
from anything.** The completed stock record documents no frame penetration that
our harness has to cross, and stock ran its own nine signal home-runs through
the same pad without any recorded edge protection.

So there is nothing to pre-buy. **What a grommet is for, and when to add one:**
a cable that passes through a drilled hole or over a cut edge in the steel frame
will have its insulation sawn through by vibration — and a dance pad is a
vibration machine by definition. It is a slow failure that presents as an
intermittent panel months later.

**At assembly, walk each run and protect anywhere cable meets a cut steel edge.**
A grommet, a short length of split loom, or a few wraps of self-amalgamating
tape all work; the point is that bare cable never bears on bare metal. This is a
five-minute inspection, not a BOM line.

## Identification

Per-panel identification is **two bands of coloured heat-shrink at both cable
ends**, not conductor color — the chosen RVSP cable comes in one color only.

**Row + column, six colours, decided 2026-08-17.** Rows top→bottom
**red/yellow/green**; columns left→right **blue/white/violet**. So `UL` is
red+blue, `C` is yellow+white, `DR` is green+violet. The sets are disjoint, so
the band order does not matter. **This replaced the stock 0=Red … 8=Black map**,
which needed nine distinguishable colours that are not sold. Full scheme and the
substitution rules: `WIRE_COLORS.md` → "Panel identification".
