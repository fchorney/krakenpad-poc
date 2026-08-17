# Wire Colors — our harnesses

Conductor colors for the harnesses this project builds. The stock pad's colors
are a separate record: `stock-smx/WIRE_COLORS.md`.

Status column: **confirmed** = settled. **TODO** = not yet chosen.

The two-letter codes are WireViz's, used in the `.yml` sources:
`BK` black, `WH` white, `RD` red, `GN` green, `YE` yellow, `BU` blue,
`BN` brown, `PK` pink, `GY` grey, `VT` violet, `OG` orange.

**Per-panel identification is colored heat-shrink at BOTH cable ends, not
conductor color** — the chosen RVSP cable comes in one color only. **The scheme
is two bands encoding row + column** (below), which replaced the stock SMX
0=Red … 8=Black map on 2026-08-17.

## Panel identification — two-band row/column code

**DECIDED 2026-08-17.** The stock map needed **9 distinct colors**, and coloured
heat-shrink is simply not sold in nine distinguishable colours — that was the
blocker. This scheme needs **6**, as two disjoint sets of three, and every cable
end carries **two bands: one row, one column.**

| | col 1 (**L**) | col 2 (**C**) | col 3 (**R**) |
|---|---|---|---|
| **row 1 (U)** | `UL` = **red + blue** — ID 0 | `U` = **red + white** — ID 1 | `UR` = **red + violet** — ID 2 |
| **row 2 (M)** | `L` = **yellow + blue** — ID 3 | `C` = **yellow + white** — ID 4 | `R` = **yellow + violet** — ID 5 |
| **row 3 (D)** | `DL` = **green + blue** — ID 6 | `D` = **green + white** — ID 7 | `DR` = **green + violet** — ID 8 |

- **Rows, top → bottom: red, yellow, green.** A traffic light read downward.
- **Columns, left → right: blue, white, violet.**

### Why this is better than the 9-colour map it replaces, beyond needing fewer colours

- **It encodes physical position, and so does the master's silkscreen.** J3–J11
  are labelled `UL`/`U`/`UR`/`L`/`C`/`R`/`DL`/`D`/`DR`, so a cable now reads in
  the same terms as the socket it plugs into. The old map encoded *panel ID*,
  which is a DIP-switch/firmware property the master doesn't even assume is
  correct — it learns the real mapping with the `'I'` identify pulse.
- **Order-independent, because the two sets are disjoint.** Red is only ever a
  row and blue only ever a column, so "red then blue" and "blue then red" decode
  identically. There is no way to misread a cable by looking at the bands in the
  wrong order — worth preserving if the colours are ever changed.
- **Degrades gracefully.** Lose a band and you still have a row *or* a column,
  narrowing to three candidates. Lose the single band of a 9-colour scheme and
  you have nothing.

### Rules if the colours get substituted

The exact colours matter less than these three properties. Substitute freely
from what is actually in the bin, but keep:

1. **The two sets disjoint** — no colour used as both a row and a column, or the
   order-independence above is lost.
2. **No black and no clear.** Black is the default heat-shrink colour *and* the
   RVSP jacket is dark, so a black band reads as ordinary strain relief; clear
   reads as unmarked. This is why the stock map's `8 = Black` could not survive.
3. **No confusable pair across the sets** — red/orange, blue/violet,
   yellow/white, brown/black. Inside a closed pad the light is bad. Note the
   proposed sets already put blue and violet in the *same* set, where they are
   only ever compared against each other's positions, not against a row.

**Quantity: 2 bands × 9 cables × 2 ends × 2 pads = 72 pieces**, ~8 of each
colour per pad. Mark both ends of every cable identically.

**Printed labels under clear shrink remain the gold standard** — `UL` needs no
lookup table at all — and stay the recommended upgrade if a label printer is
ever to hand. This scheme exists because nine colours are unobtainable, not
because colour-coding is preferable.

## 12V trunk and fan-out — `12v-trunk.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| +12V, XT30 to fuse | 20 AWG | red | `RD` | confirmed |
| +12V, fuse to Wago | 20 AWG | red | `RD` | confirmed |
| GND, XT30 to Wago | 20 AWG | black | `BK` | confirmed |

Taken from the same 2C 20 AWG jacketed reel as the power columns. The PSU's own
captive cable upstream of the XT30 is **18 AWG and BOTH CONDUCTORS ARE BLACK** —
one carries a moulded ridge, and which is +12 V is not recorded. Meter the
barrel before cutting it. See `stock-smx/harness/psu-12v.yml`.

## 12V power columns — `power-column.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| +12V | 20 AWG | red | `RD` | **TODO confirm** |
| GND | 20 AWG | black | `BK` | **TODO confirm** |

## RS-485 chain — `rs485-chain.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| A | 22 AWG | blue | `BU` | **TODO confirm** |
| B | 22 AWG | white | `WH` | **TODO confirm** |
| Shield | — | drain | — | pin 3, both ends |

## INT home runs — `int-home-run.yml`

New builds, not stock — colors are ours to choose.

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| INT | 22 AWG | white | `WH` | proposed |
| GND | 22 AWG | black | `BK` | proposed |

Per-panel identification is **two bands of coloured heat-shrink at both cable
ends**, not conductor color — the chosen RVSP cable comes in one color. See
"Panel identification — two-band row/column code" at the top of this file. The
stock SMX 0 = Red … 8 = Black map is **superseded** (2026-08-17) and no longer
names markers.

## FSR leads — `fsr-panel.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| S1 | 30 AWG | red | `RD` | confirmed — sensor's own moulded tail |
| S2 | 30 AWG | black | `BK` | confirmed — sensor's own moulded tail |

The FSR is not polarised, so these two are interchangeable electrically — the
colors matter only for build consistency.

## Underglow — `underglow.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| 12V | 22 AWG | yellow | `YE` | confirmed at teardown (pad side) |
| DATA | 22 AWG | white | `WH` | confirmed at teardown — **not pink**; the Gen4+ manual's pink was never observed |
| GND | 22 AWG | black | `BK` | confirmed at teardown (pad side) |
| Master GND tie | 20 AWG | black | `BK` | new build |
