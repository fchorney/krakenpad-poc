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

`PARTS.md` is the connector/contact/cable/tooling index **and the shopping
list** — everything still to buy is marked ⬜ there.

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
| RS-485, 22 AWG shielded pair | **4.2 m** | **15–20 m** |
| INT home runs, 24 AWG shielded pair | **9.3 m** | **30 m — not 20 m** |

⚠ **The "Needed" column is one pad; the "Buy" column is two.** These were
mismatched until 2026-08-16, when the buy quantities were still sized for a
single pad even though the board and connector orders had long been sized for
two. Sourcing detail and the match-checks are in `docs/BOM.md` → Order 4.

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
