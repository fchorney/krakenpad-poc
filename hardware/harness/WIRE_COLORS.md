# Wire Colors — our harnesses

Conductor colors for the harnesses this project builds. The stock pad's colors
are a separate record: `stock-smx/WIRE_COLORS.md`.

Status column: **confirmed** = settled. **TODO** = not yet chosen.

The two-letter codes are WireViz's, used in the `.yml` sources:
`BK` black, `WH` white, `RD` red, `GN` green, `YE` yellow, `BU` blue,
`BN` brown, `PK` pink, `GY` grey, `VT` violet, `OG` orange.

**Per-panel identification is a marker at BOTH cable ends, not conductor
color**: the chosen RVSP cable comes in one color only. **The scheme is a
printed label under clear heat-shrink** (below), decided 2026-08-31 and used on
both pads. It replaced a two-band row/column colour code (unused, kept in
`docs/archive/INT_COLOUR_CODE_FALLBACK.md`), which had replaced the stock SMX
0=Red … 8=Black map.

## Panel identification — printed labels under clear shrink

**DECIDED 2026-08-31.** Each cable end carries a printed slip reading the panel's physical
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
reel has foil + braid and no drain wire — `PARTS.md` → "Cable"). Load both pieces onto
the conductor in the same pass.

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
| +12V | 20 AWG (measures 21) | red | `RD` | confirmed on the built cables, 2026-10-20 |
| GND | 20 AWG (measures 21) | black | `BK` | confirmed on the built cables, 2026-10-20 |

## RS-485 chain — `rs485-chain.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| A | 22 AWG (measures 24) | red | `RD` | confirmed on the built cables, 2026-10-20 |
| B | 22 AWG (measures 24) | black | `BK` | confirmed on the built cables, 2026-10-20 |
| Shield | — | green | `GN` | confirmed on the built cables, 2026-10-20. Micro-Fit pin 3, both ends |

## INT home runs — `int-home-run.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| INT | 22 AWG (measures 24) | red | `RD` | confirmed on the built cables, 2026-10-20 |
| GND | 22 AWG (measures 24) | black | `BK` | confirmed on the built cables, 2026-10-20 |

⚠ **Red is the signal here, not a supply.** Both RS-485 and INT use the same
red/black reel, so on these cables red means RS-485 A or INT signal. Only the
power columns carry 12 V on red. The connectors keep them apart: 2-circuit
Micro-Fit for power, 3-circuit for RS-485, JST XH and the screw terminal for
INT.

Per-panel identification is a **printed position label under clear heat-shrink
at both cable ends**, not conductor color. See "Panel identification" at the top
of this file.

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
