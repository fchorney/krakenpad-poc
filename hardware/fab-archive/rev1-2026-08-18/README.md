# REV 1 — the exact files sent to fab, 2026-08-18

> # 🔒 FROZEN. DO NOT EDIT ANYTHING IN THIS DIRECTORY.
>
> **These are the byte-exact artifacts that were manufactured.** They are archived
> here because `hardware/**/production/` is gitignored as a build artifact, so
> without this directory there would be **no record in git of what was actually
> built** — which is exactly what you need when debugging a physical board.
>
> Regenerating from source *should* reproduce these, but `kicad-cli` and KiKit
> versions drift. When a board misbehaves, compare against **these** files, not a
> fresh export.
>
> ⚠ **One caveat, found 2026-08-21 (`docs/CAM_REVIEW.md` §2).** "Byte-exact" holds
> for **master-pcb** — all 14 files match what JLC received. It does **not** hold
> literally for **panel-gerbers.zip**: JLC's copy is the `08:28:44` export and this
> one is a `10:05:44` re-export of the same board, so all 16 files differ by
> timestamp and by a copper-pour re-fill whose boundary moves **≤ 2.5 µm**. Every
> pad, trace, via, drill, mask and silk feature is identical. Do not mistake that
> drift for a real change.

## What was ordered

| | |
|---|---|
| **dual-panel** | ×20, 4-layer, **ENIG**, POFV, SMD-only PCBA both sides (101 top / 14 bottom, 115 placements, 35 BOM lines), bake, board cleaning |
| **master-pcb** | ×5, 4-layer, **OSP**, bare fab only — no PCBA, no bake, no POFV |
| Panel size | 227.57 × 143.10 mm (326 cm²), customer panel, one carrier + one brain per panel |
| Cost | $630.93 USD JLC ($876.05 CAD) + $155.80 USD LCSC ($216.33 CAD), plus a **$24.42 USD / $33.91 CAD panelisation supplement** charged after review for carrying two designs on one panel |

Full quote and reasoning: `hardware/dual-panel/panel/QUOTE-2026-08-18.md`.
What was said to JLC: `docs/ORDER_NOTES.md`.

## Verify a regenerated package against what was built

```sh
cd hardware/fab-archive/rev1-2026-08-18 && shasum -a 256 -c SHA256SUMS.txt
```

## Design state at fab

Both boards were clean at the moment of ordering:

| board | DRC | unconnected | parity | ERC |
|---|---|---|---|---|
| dual-panel | 0 | 19 ✱ | 90 ✱✱ | 0 |
| master-pcb | 0 | 0 | 0 | 0 |

✱ the permanent board-to-board mating-gap floor — KiCad cannot model "these mate".
✱✱ a `kicad-cli`-only net-*naming* artifact; the GUI reports none and the exported
netlist agrees with the PCB pad-for-pad across all 577 pads.

## Changes made on order day — the likeliest suspects if something is wrong

All four were verified before ordering, but they are the youngest changes in the
design and therefore where to look first:

1. **U306 RP2040 signal pads widened 0.20 → 0.23 mm** (local footprint
   `dual-panel:QFN-56-1EP_7x7mm_P0.4mm_EP3.2x3.2mm_RPiPad`), to match Raspberry
   Pi's official land pattern. Pad length, pitch, centres and the 3.2 mm EP
   unchanged.
2. **RS-485 transceiver THVD1429 → THVD1450DR** (`C1850236` → `C2671361`), both
   boards. Identical SOIC-8 pinout. **Gives up on-die IEC 61000-4-5 surge
   protection**; the A/B pair has no external TVS (D201 is on the INT line).
3. **10 nF ADC filter caps `C57112` → `C723749`** — the old part was X7R while the
   schematic said C0G. C324/C326/C329/C330, the four FSR ADC inputs.
4. **master-pcb: 297 vias enlarged 0.45 → 0.60 mm** (0.075 → 0.150 mm annular),
   after DFM was run on the master for the first time.

## Known-accepted DFM findings — not defects

`docs/ORDER_NOTES.md` §3, §8 and §11 carry the full arguments. Summary: the USB
differential pair's 0.14 mm gap is intentional (sets impedance), U306's "pin inner
edge" is flush with the pin at the datasheet's **maximum** terminal length only,
via-in-pad is deliberate and POFV is the mitigation, and `tht to smd` does not
apply because THT is hand-soldered.

## Deferred to rev 2

- **External TVS on RS-485 A/B** — decided 2026-08-18, purely additive.
- RP2040 pad *length* 0.875 mm vs Raspberry Pi's 0.80 mm (deliberate, extra toe).
