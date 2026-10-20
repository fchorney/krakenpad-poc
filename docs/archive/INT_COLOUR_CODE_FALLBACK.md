> **ARCHIVED 2026-10-20 — not used.** Both pads were built with printed labels under
> clear heat-shrink (`hardware/harness/WIRE_COLORS.md` → "Panel identification").
> This was the documented fallback marker scheme, moved out of that file. Its three
> rules (disjoint sets, no black or clear, no confusable pair) still apply to any
> colour marking added to this harness later.

# INT cable colour code — two-band row/column (fallback, unused)


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

