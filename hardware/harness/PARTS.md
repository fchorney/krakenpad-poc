# Harness parts — as built

Connectors, contacts, cable and tooling for the harnesses **this project
builds**. Both pads' harnesses were built by 2026-10-12. The stock pad's parts
are a separate record: `stock-smx/PARTS.md`.

How each choice was reached (the cable gauge and CCA measurements, ferrule
sizing, fuse and connector selection, tooling) is in
`docs/archive/HARNESS_PARTS_HISTORY.md`. The rev-1 purchase record, with LCSC
quantities and prices, is `docs/rev1-order/BOM.md`.

> ⚠ **Read `stock-smx/PARTS.md` → "Connector gender conventions" before
> ordering any housing.** JST and Molex use "receptacle" to mean *opposite*
> contact genders, and this pad contains both vendors. **JST `xxR` = receptacle
> = male pins; JST `xxP` = plug = female sockets; Molex is the exact opposite.**

## Board-side (on the PCBs, listed for mating reference)

| Ref | Part | LCSC | Used by |
|-----|------|------|---------|
| Master J1 | Micro-Fit 3.0 3-pin RA header (436500300) | C503478 | `rs485-chain.yml` |
| Master J2 | KANGNEX WJ500V-5.08-2P screw terminal | C8465 | `underglow.yml`, GND tie |
| Master J3–J11 | JST B2B-XH-A, 2-pin vertical | C158012 | `int-home-run.yml` |
| Carrier J214 | KANGNEX WJ500V-5.08-2P screw terminal | C8465 | `int-home-run.yml` |
| Carrier J201/J202/J203/J206 | JST B2B-PH-K-S, 2-pin top entry | C131337 | `fsr-panel.yml` |
| Carrier J204/J207 | Micro-Fit 3.0 3-pin RA header (436500300) | C503478 | `rs485-chain.yml` |
| Carrier J205/J208 | Micro-Fit 3.0 2-pin RA header (436500200) | C192562 | `power-column.yml` |

The carrier↔brain interface (`J210`–`J213` headers, `J301`–`J304` sockets) is
not a harness part. It stacks to **11.04 mm**, so the carrier and brain are
spaced with **12 mm** M3 female-female standoffs, three per panel. The spacer is
load-bearing: without it the connectors take the clamping force. See
`docs/DUAL_PANEL.md` → "Mechanical stack".

## Cable-side, per pad

Double these for two pads.

| Part | For | LCSC / source | Per pad |
|------|-----|------|-------------|
| JST XHP-2 housing | INT, master end | C144401 | 9 |
| JST SXH-001T-P0.6N contact, 22–28 AWG | INT, master end | C385122 | 18 |
| Molex 436450200 Micro-Fit 2-ckt receptacle housing | 12 V | C114089 | 15 (3 feeds × 1 + 6 jumpers × 2) |
| Molex 436450300 Micro-Fit 3-ckt receptacle housing | RS-485 | C259740 | 18 |
| Molex 430300001 Micro-Fit crimp, 20–24 AWG | 12 V + RS-485 | C259786 | 84 (15 × 2 + 18 × 3) |
| 3-pin SM 2.5 LED-strip pigtail, 22 AWG | underglow | pre-made, on hand | 1 |
| Wago 221-415, 5-way lever block | 12 V fan-out | 10-pack | 3 |
| XT30U-F (PSU side, recessed sockets) | PSU → trunk | C99102 | 1 |
| XT30U-M (exposed pins) | our trunk + the cut stock tail | C99101 | 2 |
| Inline 5×20 mm fuse holder, **18 AWG leads** | trunk | Amazon | 1 |
| **T8AL250V** glass cartridge, time-lag, IEC 60127 | trunk | Amazon | 1 |
| Ferrule, bare, **0.34 mm²** (0.8 mm ID) | J214 INT conductors | domestic | 18 |
| Ferrule, bare, **0.5 mm²** (1.0 mm ID) | J2: GND tie (20 AWG, checked), underglow DATA (22 AWG, by reasoning; test-fit not recorded) | on hand | 2 |
| Printed label under clear heat-shrink | per-cable identification, both ends | — | 18 |

Wago lever blocks take bare stranded conductor and want **no ferrule**.

## Cable

| Run | Cable | Marked | **Measured** (2026-08-31) |
|---|---|---|---|
| 12 V trunk + columns | 2-conductor jacketed, red/black | `STYLE 2464 20AWG`, 80 °C 300 V | **21/0.16 copper, 0.42 mm² ≈ 21 AWG** |
| RS-485 **and** INT (one reel) | RVSP 2-core shielded twisted pair, foil + braid, **no drain wire** | `ZC-RVSP 2x0.3mm²`, sold as 22 AWG, 70 °C | **16/0.13 copper, 0.212 mm² = 24 AWG** |
| Underglow adapter | 3-conductor 22 AWG, spliced onto the SM pigtail | — | — |

Both cables are copper and one gauge under their marking. Neither matters
electrically. The RVSP sits at the **bottom edge** of the Micro-Fit crimp's
20–24 AWG range: the crimps **pass a pull-test, but only just** (2026-09-08).
That is accepted for these two prototype pads. For any larger build, source
genuine 22 AWG specified by **measured insulation OD (1.30–1.85 mm) and
conductor resistance**, not by a printed gauge.

Cable lengths per run are in `README.md` → "Cable quantities".

## Assembly rules

- **Slide heat-shrink on before crimping.** After termination the only retrofit
  markers are tape (rejected: it unwinds and its adhesive migrates) or clip-ons.
- **Pull-test every Micro-Fit crimp.** See the cable note above.
- **RS-485 shield → Micro-Fit pin 3.** Gather the braid, solder a short 22 AWG
  lead, heat-shrink the joint, crimp that lead. Never tin the part inside a
  crimp barrel.
- **INT shield: trim and heat-shrink at both ends, never bond it to the INT GND
  conductor.** That conductor is the signal return; a shield across it restores
  the loop area the twist removes.
- **Never tin a conductor as a ferrule substitute.** Solder cold-flows under the
  screw and the joint loosens.
- **Underglow pinout: pin 1 = GND, pin 2 = DATA, pin 3 = 12 V.** Reversed, 12 V
  lands on DATA and the first LED dies. Label the tails while splicing.
- **XT30: the female half goes on the PSU** (recessed sockets on the
  always-live side). Confirm by looking at the parts, not the label.
- **Protect cable wherever it bears on a cut steel edge** (grommet, split loom or
  self-amalgamating tape). No frame penetration was found at teardown, so this
  is an assembly-time inspection, not a part.

## 12 V fan-out

Three Wago 221-415 blocks per pad: **+12 V on one block** (trunk in, three
column feeds, underglow) and **GND on two jumpered blocks** (the same five plus
the master GND tie, which gets its own port so unplugging underglow cannot break
the RS-485/INT reference). Rated 32 A / 12 AWG, accepts 24–12 AWG. Full harness:
`12v-trunk.yml`.

The fuse is IEC time-lag. **Do not substitute a UL "slow blow" 8 A**: UL rates
current differently, and a UL 8 A part is good for only ~6 A continuous, below
the 6.34 A worst case.

## Tooling

| Tool | For |
|---|---|
| IWISS SN-28B | Micro-Fit 3.0 crimps |
| Engineer PA-09 | JST XH crimps |
| HSC8 6-4 self-adjusting ferrule crimper | ferrules. ⚠ **Its adjustment pin was tightened** to crimp 0.34 mm² properly; the 0.5 mm² size was checked good at the same setting. The change isn't visible from outside the tool |
| Molex **11-03-0043** extraction tool | Micro-Fit 3.0 rework. Not `-0044`, which is the Mini-Fit Jr. one |

## Master USB to the PC

No parts. The pad's shell-mounted USB-C port is wired internally to a Micro-B
plug that went to the stock Arduino Micro, and the Teensy 4.0 takes the same
plug.

## Open

- **The stock internal USB cable** has not been recorded as tested with the
  master: confirm it carries data and that the Teensy still negotiates USB High
  Speed through it. If HS fails in the pad but not on the bench, suspect this
  cable first.
- **Printed fan-out carrier** (PETG, on the Daygreen's two M3 holes, 57 mm
  centres; engraved `+12V`/`GND`; lever pockets that clear a full swing). Not
  recorded here whether it has been built.
- **JST `SYM-001T-P0.6` SM male pins** (28–22 AWG) for the `SMR-03V-B` housings
  (`C157907`, 10 on hand), so underglow leads can be made to any length instead
  of splicing a pre-made pigtail. Wanted, not urgent; LCSC part and stock not
  checked. Do **not** order `SMM-003T-P0.5`: it is 28–30 AWG.
- **Fuse-holder current rating** should be ≥10 A; the lead gauge doesn't prove
  it. Not recorded as checked.
