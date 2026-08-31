# Wire Colors — our harnesses

Conductor colors for the harnesses this project builds. The stock pad's colors
are a separate record: `stock-smx/WIRE_COLORS.md`.

Status column: **confirmed** = settled. **TODO** = not yet chosen.

The two-letter codes are WireViz's, used in the `.yml` sources:
`BK` black, `WH` white, `RD` red, `GN` green, `YE` yellow, `BU` blue,
`BN` brown, `PK` pink, `GY` grey, `VT` violet, `OG` orange.

**Per-panel identification is a marker at BOTH cable ends, not conductor
color** — the chosen RVSP cable comes in one color only. **The scheme is a
printed label under clear heat-shrink** (below), decided 2026-08-31 when the
reel landed. The **two-band row/column colour code** is the documented fallback,
and it in turn replaced the stock SMX 0=Red … 8=Black map on 2026-08-17.

## Panel identification — printed labels under clear shrink

**DECIDED 2026-08-31, and it supersedes the colour code below as the primary
scheme.** Each cable end carries a printed slip reading the panel's physical
position — `UL` `U` `UR` `L` `C` `R` `DL` `D` `DR` — captured under a piece of
**clear** heat-shrink.

**Why it won, when the colour code was already designed and costed:**

- **No lookup table.** `UL` is self-describing; `red + blue` is not. Every
  decode of the colour scheme is a trip back to this file.
- **It matches the socket.** The master's J3–J11 silkscreen already reads
  `UL`/`U`/`UR`/…, so the label and the socket are the *same string*. The colour
  code only encoded the same information indirectly.
- **The bin was one colour short anyway.** Stock on hand is red, blue, green,
  white, yellow, black and clear — **no violet**, and black and clear are both
  excluded as markers (see rule 2 below). The colour scheme needed a purchase;
  this one uses clear shrink already on the shelf.
- **Immune to bad light.** The failure mode the colour code fights hardest —
  telling blue from violet inside a closed pad — does not exist here.

**Practical rules:**

- **Laser-print the slips, not inkjet** — toner will not smudge or run when the
  shrink is heated; inkjet ink can. No printer to hand? A fine **paint pen on
  white heat-shrink** is the acceptable substitute; a permanent marker directly
  on shrink rubs off with handling and is not.
- **Print the position, never the panel ID.** Slot↔ID agreement is not assumed
  anywhere in this project — the master learns the real mapping with the `'I'`
  identify pulse and reports mismatches (`docs/RS485_PROTOCOL.md`).
- **Quantity: 9 cables × 2 ends × 2 pads = 36 labels**, half the 72 pieces the
  two-band code needed.
- Mark both ends of every cable identically.

### ⚠ Slide the shrink on BEFORE crimping

This applies to either scheme and is the one ordering mistake that cannot be
undone. Heat-shrink has to go onto the cable **before** the connectors are
terminated. Crimp all 204 contacts first and the only retrofit markers left are
tape or clip-on sleeves — and **tape is rejected here** (adhesive migrates and
the wrap unwinds in a warm pad), which is the whole reason it is not the scheme.

Note the RS-485 ends also need heat-shrink for the **braid-to-drain joint** (the
reel has foil + braid and no drain wire — `docs/BOM.md`). Load both pieces onto
the conductor in the same pass.

## Fallback — two-band row/column colour code

**Decided 2026-08-17; demoted to fallback 2026-08-31.** Kept in full because it
is what to build if labels prove impractical at assembly, and because its design
rules constrain any colour marking used anywhere in this harness.

The stock map needed **9 distinct colors**, and coloured
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

**This scheme existed because nine colours are unobtainable, not because
colour-coding was ever preferable** — which is exactly why the printed labels
above took over as soon as the alternative was actually on the shelf.

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
