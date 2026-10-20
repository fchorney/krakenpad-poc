# Rev 1 — how the boards in use were verified, ordered and bought

**Rev 1 is the current hardware.** Every master and panel on the bench is a
rev-1 board, ordered from JLCPCB on 2026-08-18 (tag `rev1-fab`). This folder is
the frozen record of that order. Nothing in it is a live instruction, but it is
the answer to "why is the board in my hand like this?"

For the next order, the reusable procedure is `docs/FAB_ORDER.md`. For what
rev 2 might change, see `docs/REV2_CANDIDATES.md`.

| File | What it records |
|---|---|
| `PRE_ORDER_CHECKLIST.md` | Every check run before paying: fab package, DRC/DFM triage, LCSC part-identity and JLC assembly-stock audits |
| `ORDER_NOTES.md` | What was said to JLC: POFV wording, `J305`, the intentional DFM findings, the order-form remarks, the post-order emails |
| `CAM_REVIEW.md` | JLC's production files compared against what was sent (2026-08-21) |
| `BOM.md` | The rev-1 purchase: all four orders, the cart audit, the cable acceptance measurements |
| `QUOTE-2026-08-18.md` | The real qty-20 quote, and the landed cost including import charges |
| `QUOTE-2026-07-31.md` | The earlier qty-5 quote, kept for the single-vs-dual board comparison |
| `POWER_PROTECTION.md` | **Decided 2026-08-17:** no board-level reverse-polarity or per-panel fuse on rev 1, and why. Ends with notes for a rev-2 revisit |
| `review/` | The r/PrintedCircuitBoard post drafts and rules checklist. The images are regenerated with `tools/gen_review_images.py` |

The byte-exact files that were manufactured are in
`hardware/fab-archive/rev1-2026-08-18/`, not here.
