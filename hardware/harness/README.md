# Wire Harness — our design

Source-of-truth for every cable, connector and pigtail **this project builds**.

> **The stock SMX pad's wiring is a separate record: [`stock-smx/`](../../stock-smx/).**
> That tree is descriptive and stands on its own as working knowledge of stock
> hardware. This one is prescriptive. **Do not merge them** — a stock file must
> stay true and useful to someone repairing a factory pad who has never heard of
> this project.
>
> Where a stock finding bears on one of these harnesses, the note goes **here**,
> never in the stock file.

Diagrams are generated with [WireViz](https://github.com/wireviz/WireViz): each
`.yml` is a text description of connectors + cable + connections, and WireViz
renders an SVG/PNG harness drawing and a BOM from it.

## Regenerating

```sh
uv tool install wireviz     # one-time; needs graphviz (brew install graphviz)
./gen.sh                    # renders every .yml to out/
```

Outputs land in `out/` and are **not** committed.

## The harnesses

| File | Harness | Count per pad |
|------|---------|---------------|
| `12v-trunk.yml` | PSU → XT30 → inline fuse → Wago lever-block fan-out | 1 |
| `power-column.yml` | Fan-out → column of 3 panels, daisy chain | 3 |
| `rs485-chain.yml` | Master → 9 panels, serpentine RS-485 chain | 1 |
| `int-home-run.yml` | Master J3–J11 → nine panel carriers, home runs | 1 |
| `underglow.yml` | Fan-out + master → underglow strips | 1 |
| `fsr-panel.yml` | FSR → panel carrier, internal to a panel | 36 (4×9) |

`PARTS.md` is the as-built connector, contact, cable and tooling list, with the
assembly rules and the few open items. Both pads' harnesses were built by
2026-10-12.

`WIRE_COLORS.md` is the per-conductor color reference.

## Topology

```
AC (stock, unchanged) → YU1208 12V 8.5A (retained)
  → XT30 (replaces the stock 5.5×2.5 barrel; the barrel's other half
     gets the second XT30 so the stock harness can be plugged back in)
  → T8A slow-blow inline fuse
  → Wago 221-415 lever blocks, mounted on the Daygreen's own M3 holes
      ├─ 20 AWG → Micro-Fit 2p → panel 0 → 3 → 6
      ├─ 20 AWG → Micro-Fit 2p → panel 1 → 4 → 7
      ├─ 20 AWG → Micro-Fit 2p → panel 2 → 5 → 8
      ├─ 22 AWG → SM 3P → underglow strips
      └─ GND rail only: master GND tie (its own port, its own lead)
```

Three lever blocks: **one on +12 V** (exactly 5 ports) and **two jumpered on
GND** (6 ports — the extra is the master GND tie). The external 12 V cabinet
input is deferred and not designed in; that is what makes the +12 V count land
at 5.

## What we keep, and what we replace

**Retained unmodified:** the whole AC side (wall cord, YD06 EMI filter, locking
C13), the YU1208 supply itself, and its captive 18 AWG output cable.

**Removed:** the 5.5 × 2.5 barrel pair, the 30 cm 20 AWG tail and its JST YL
2-way, the Daygreen converter, and everything downstream of it at 5 V.

**Why both connectors go:** every connector the stock pad offers at that point
is a ~5 A class part, against our 6.34 A. A JST YL at 2 circuits and #20 AWG is
rated **5.0 A**; a 5.5 × 2.5 barrel is 3–5 A. Both would be permanent
bottlenecks below the load. With them gone, **nothing in the 12 V path limits
below the supply's own 8.5 A**.

**Cutting is still reversible** — the second XT30 goes on the cut stock tail, so
the original 12 V path can be restored by unplugging our trunk and plugging the
stock tail back in. Label it and keep it with the pad.

⚠ **Meter the barrel's polarity before cutting it.** It is the only unambiguous
polarity reference on the supply side — both captive conductors are black and
the moulded ridge's meaning is *not recorded*. Power the supply, meter centre
pin vs sleeve (label says centre-positive), note which conductor is +12 V, *then*
cut. Detail in `12v-trunk.yml`.

## Cable quantities

All three come from the completed stock record, since our harnesses run the same
geometry through the same panel positions. The WireViz BOMs in `out/` regenerate
these totals.

| Cable | Needed **per pad** | Buy (build scope = **2 pads**) |
|---|---|---|
| 12 V columns + trunk, 2C 20 AWG | 5.4 m + ~1 m = **6.4 m** | **20 m** |
| **RS-485 + INT, 22 AWG 2-core shielded pair (RVSP)** — one reel | 4.2 m + 10.65 m = **14.85 m** (INT was 9.3 m before the +15 cm/cable of 2026-10-02) | **40 m** (bought 50 m) |

**As built (both pads, 2026-10-12):** power 18 × 60 cm; RS-485 2 × 60 cm +
16 × 45 cm; INT per `int-home-run.yml`. A dry fit found the stock-derived power
and RS-485 lengths a little long, and they were kept. Two INT pairs share a
length (UL/C at 105 cm, L/D at 135 cm), so each pair was labelled one of each;
they are interchangeable. On one pad (which one isn't recorded; the pads have
no A/B names), the INT cable first cut for `U` at 90 cm was relabelled `UR`.

### Install findings, 2026-10-22 (the bench pad, master #2; reported by the user)

All the stock electronics came out and the new set went in, in a few hours.
Nothing failed, and the 12 V chain went in as designed (`docs/BRINGUP_LOG.md`
→ master #2, 2026-10-22). What the install showed about the harness:

1. **Almost every cable is a little long.** All nine INT cables are
   *quite* long. Power and RS-485 could each be a little shorter, and the two
   RS-485 jumpers that run between columns, `DL`→`D` and `U`→`UR`, could be
   shorter still: they need no slack (confirmed by the user). **New lengths:
   TODO, not measured yet.**
2. **The one cable that was too short is RS-485 master → panel 0 (`UL`),
   60 cm.** That is the length the dry fit said to check once the master was
   in (`rs485-chain.yml`). **New length: TODO, not measured yet.**
3. **The INT end labels make the panel end hard to plug in.** There is little
   room at `J214`, and the printed label under clear heat-shrink stiffens
   the end of the cable so it barely bends. The labelling scheme
   (`WIRE_COLORS.md`, decided 2026-08-31) needs either a different method or a
   different place for the label. **Open: not decided.**
4. **The unjacketed length at each end should be longer.** At the
   connectors there is little room to work, and the jacket's protection is
   needed further along, where the cable runs through the pad. The current
   strip length isn't recorded anywhere in this tree. **New length: TODO.**

Whether either pad's harness gets rebuilt to these is the user's call; until
new lengths are measured, the figures above (and in the `.yml` files) are
still as built.

**RS-485 and INT share one cable as of 2026-08-16.** Both are 2-core shielded
twisted pair at signal-level current; only the shield termination differs
(RS-485 → Micro-Fit pin 3, INT trimmed). 22 AWG is the only gauge inside both
connector windows — JST XH needs ≥1.30 mm insulation OD, Micro-Fit ≤1.85 mm.
Rationale in `docs/archive/HARNESS_PARTS_HISTORY.md` → "RS-485 and INT share
ONE cable". The cable as delivered measures 24 AWG; see `PARTS.md` → "Cable".

⚠ **The "Needed" column is one pad; the "Buy" column is two.** These were
mismatched until 2026-08-16, when the buy quantities were still sized for a
single pad even though the board and connector orders had long been sized for
two. Sourcing detail and the match-checks are in `docs/rev1-order/BOM.md` → Order 4.

## Power budget

**≈ 6.34 A / 76 W** at full white everywhere, against an 8.5 A supply = 75%
loaded — and that is the *datasheet* worst case, a state nothing ever commands.
Realistic peak using measured figures is 5.30 A.

| Load | Math | Amps @ 12 V |
|---|---|---|
| Panel LEDs | 9 × 25 × 15 mA (WS2815, **per pixel**) | 3.4 |
| Panel brains | 9 × ~60 mA | 0.5 |
| Underglow | 44 groups × 55.5 mA (WS2811, 3 × 18.5 mA) | 2.44 |
| Master | USB-powered | 0 |
| | **total** | **≈ 6.34 A** |

⚠ **The two LED families behave in opposite ways.** WS2815 stacks three dies in
series and shorts unlit ones, so **panel red draws exactly what white does** and
only PWM duty reduces current. WS2811 has three independent sinks, so
**underglow red draws ⅓ of white** and colour scales current linearly. A single
global power or animation policy is therefore wrong. Full treatment and both
current models: `docs/UNDERGLOW.md` → "Current draw".

### WS2815 behaviour worth knowing

Moved here from `CLAUDE.md` on 2026-10-20.

- **12V buys BOTH lower current and lower power.** Bench A/B, one LED at full white so neither strip is voltage-sagging: **WS2815 0.108W vs WS2812B 0.247W — ~44% of the power for comparable brightness** (brightness confirmed by eye at 25 LEDs). Current is also **2.7× lower**, which is the harness win. The old "~2× the power per LED, because it linearly drops ~9V as heat" claim is **wrong and superseded**: three dies in series total ~8.4V, so the regulator drops ~3.6V, not 9V. Compare per-LED figures only at equal, unsagged rail voltage — a 25-LED WS2812B comparison read 3.59W vs 3.16W and looked like a wash purely because the 5V strip had drooped to 4V
- **WS2815 is constant-current; WS2812B is not, and the bench showed the difference plainly.** The WS2812B strip's per-LED draw fell from 49.3mA (1 LED) to 28.7mA (25 LEDs) as its rail sagged, and its white÷red ratio compressed from the expected 3.0 to 2.24. The WS2815 showed none of this — red exactly equalled white and 1 LED scaled cleanly to 25. **That uniformity regardless of position in the chain or drop along the harness is the reason the panels want this part**
- ✅ **Quiescent independently CONFIRMED 2026-09-08** at 1.76mA/pixel, fitted from a real panel — see the per-pixel table below. **WS2815 quiescent is high: ~1.84mA/LED** (vs ~0.07mA for WS2812B) — ~0.41A / 5W across all 225 panel LEDs with everything dark. The price of the constant-current and dual-signal circuitry. Trivial against a 10A supply, but "all off" is not free
- **Per-colour heat asymmetry — matters for animations, not for the power budget.** Because unlit dies are shorted out, pure red leaves the regulator dropping ~10V of 12V at unchanged current: ~83% of the power becomes heat for one die's worth of light. **White is the most light-efficient state on this part; saturated single colours are the least.** Current (and so PSU/wiring sizing) is identical either way, but a long saturated-colour idle animation heats the LED ICs more than the same animation in white. Not near any limit at ~4.5W/panel — just the opposite of the usual intuition
- **Corrected 2026-08-16; everything above replaces earlier figures.** The long-standing **47mA/LED** (and **36mA** before it) came from reading the datasheet's 15mA as per-channel and multiplying by three. A 25-LED cut of a `HD-12v-WS2815-144L-B-IP30` strip at a verified 11.7V measured **10.5mA/pixel, red exactly equal to white** — ~30% under datasheet, i.e. ordinary binning, and inside the community range of 8.3–13.5mA/pixel. Downstream items that moved: PSU 20A→10A, panel trunk class, F201/Q201 sizing. Bench firmware: `firmware/panel/c/led_current_test/`

### ⚠ Per-pixel figures: always say WHICH quantity

Three numbers circulate and they are **not** interchangeable. Quoting one where
another is meant has now caused the same confusion twice.

| quantity | strip cut (2026-08-16) | assembled panel (2026-09-08) | datasheet |
|---|---|---|---|
| PWM-scaled, `I × max(r,g,b)/255` | 8.7 mA | **12.9 mA** | — |
| quiescent, drawn even when dark | 1.84 mA | **1.76 mA** | — |
| **total at full white** | **10.5 mA** | **14.66 mA** | **15 mA** |

- The **budget table above uses the datasheet's 15 mA**, a *total*. The assembled
  panel measures 14.66 — so that budget is **confirmed to ~1% on hardware**
  (0.43 A/panel vs 0.44 A budgeted; 6.28 A pad vs 6.34 A), not conservative.
  **Nine panels together measured lower, 2026-10-20:** ~3.4 A at full white
  (~0.38 A/panel), against 3.87 A from that one panel's figure. The budget
  still holds as an upper bound; the per-panel number varies by ~12% between
  boards. Details: `docs/BRINGUP_LOG.md` → "2026-10-20 — full-white soak".
- The **model** for predicting what a frame draws uses the PWM-scaled figure.
  Use **12.9 mA** for the board; 8.7 was the strip and under-predicts by half.
- `8.7 + 1.84 = 10.5` and `12.9 + 1.76 = 14.66` — that is why the pairs of
  numbers in older notes look contradictory when they are not.

⚠ **The raw 2026-08-16 readings were never actually written down here**, despite
`CLAUDE.md` pointing at this section for them. What survives is the derived
figures above and the bench firmware, `firmware/panel/c/led_current_test/`. The
2026-09-08 panel readings, which *are* recorded, are in `docs/BRINGUP_LOG.md`.

⚠ **There is no current sensing anywhere in the pad.** The master is
USB-powered and deliberately outside the 12 V path, so every figure here is an
open-loop model prediction. The trunk's **inline fuse is the only current
protection**, and the stock ceiling (the Daygreen current-limiting at 75 W) is
deleted along with the converter. It is not optional.

## Conventions

- **Panel numbers are 0–8** (0 = UL … 8 = DR).
- **Lengths in metres**, as WireViz expects; bench readings are cm, so ÷100.
- Wire colors use WireViz two-letter codes (`BK` black, `RD` red, `YE` yellow,
  `GN` green, `WH` white, `BU` blue, `BN` brown, `GY` grey, `VT` violet,
  `OG` orange, `PK` pink).
- Anything not yet chosen or measured is marked `TODO` in a `notes:` field so it
  shows up on the rendered drawing rather than hiding in a comment.
- **Never write `->` or a Unicode arrow inside a `notes:` field** — graphviz
  fails with an unhelpful syntax error. Write "then".
