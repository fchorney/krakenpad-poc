# Stock Gen4+ Panel PCB — measurements from product photo

Source: Step Revolution shop product photo (`PCB-Gen4_1024x1024.jpg`), scaled by the
known 5.00" (127mm) top-to-bottom dimension → 4.65 px/mm. **Accuracy ±1–1.5mm** (JPEG,
mild lens/perspective) — good for KiCad first-draft placement; verify critical
dimensions (mounting holes especially) against the physical board with calipers before
fab. Coordinates: mm from the board's top-left corner, +X right, +Y down.

## Outline

- Main body: **127 × 127mm square (5×5")** — confirms our planned size exactly.
- Four small tabs jut out for connectors (stock frame cavity accommodates them):

| Tab | Location (Y range) | Protrusion | Stock use |
|-----|-------------------|-----------|-----------|
| Upper-left | ~16–37mm | ~3mm | 4-pin power (+5V GND GND +5V) |
| Upper-right | ~16–37mm | ~4mm | Same, mirrored (power daisy-chain) |
| Lower-left (stepped) | ~76–118mm | up to ~11mm | RJ45-style data IN + blue 2-pos screw terminal (SIGNAL/GND) |
| Lower-right | ~80–103mm | up to ~12.5mm | RJ45-style data OUT |

Implication for our board: the cavity tolerates up to ~12mm of overhang on the left and
right edges in those zones — usable room for our Micro-Fit connectors if 127mm proves
tight, but tabs are optional (stock uses them for bulky jacks we don't have).

## Mounting holes

4 holes, centers ≈ **7mm in from left/right edges, 7mm from top/bottom** (measured
(7.3, 6.5) / (119.7, 6.5) / (7.3, 119.2) / (119.7, 119.2) — design intent almost
certainly symmetric ~7mm inset). Verify with calipers; these must match the frame
standoffs exactly.

## LED lattice (25 LEDs, staggered 4–3–4–3–4–3–4)

Matches `docs/PROTOTYPE_LED_LAYOUT.md` wiring order. Regular staggered lattice,
centered on the board (measured center offset <1mm from board center):

- **7 rows, vertical pitch ≈ 16.6mm**, row 1 at Y ≈ 13mm, row 7 at Y ≈ 112–113mm
- **4-LED rows** (1,3,5,7): X ≈ 14.5, 48.0, 81.5, 115.0mm (pitch ≈ 33.5mm)
- **3-LED rows** (2,4,6): X ≈ 31.2, 65.0, 99.0mm (same pitch, offset half — diamond lattice)
- Nearest diagonal neighbor distance ≈ 23.5mm

Suspiciously close to inch-round values (33.87mm = 1.333", 16.93mm = 0.667"); within
measurement error of either. Pick clean values centered on the board for our layout —
exact stock replication isn't required (diffuser/acrylic sits well above the LEDs).

## Other stock observations

- MCU + crystal right-of-center, DIP switch bottom-right, 4 "FSR AMP" labeled zones
  (one per edge — stock amplifies FSR signals locally), bottom-center connector.
- Stock power is **+5V** on this Gen4 board (label visible) — ours is 12V, no conflict.
- The blue screw terminal labeled SIGNAL/GND is stock's interrupt/signal line — amusing
  validation of our screw-terminal INT plan.
