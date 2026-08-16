# Wire Colors — our harnesses

Conductor colors for the harnesses this project builds. The stock pad's colors
are a separate record: `stock-smx/WIRE_COLORS.md`.

Status column: **confirmed** = settled. **TODO** = not yet chosen.

The two-letter codes are WireViz's, used in the `.yml` sources:
`BK` black, `WH` white, `RD` red, `GN` green, `YE` yellow, `BU` blue,
`BN` brown, `PK` pink, `GY` grey, `VT` violet, `OG` orange.

**Per-panel identification is colored or printed heat-shrink at BOTH cable
ends, not conductor color** — the chosen RVSP cable comes in one color only.
The stock SMX panel map (`docs/BOM.md`, 0=Red … 8=Black) now names markers
rather than wires.

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
| A | 22–24 AWG | blue | `BU` | **TODO confirm** |
| B | 22–24 AWG | white | `WH` | **TODO confirm** |
| Shield | — | drain | — | pin 3, both ends |

## INT home runs — `int-home-run.yml`

New builds, not stock — colors are ours to choose.

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| INT | 24 AWG | white | `WH` | proposed |
| GND | 24 AWG | black | `BK` | proposed |

Per-panel identification is **colored or printed heat-shrink at both cable
ends**, not conductor color — the chosen RVSP cable comes in one color. The
stock SMX map in `docs/BOM.md` (0 = Red … 8 = Black) now names markers.

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
