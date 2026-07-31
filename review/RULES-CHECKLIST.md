# Pre-post checklist — r/PrintedCircuitBoard

Paraphrased from the subreddit's two pinned posts (they forbid verbatim
reproduction, so this is a restatement — read the originals before posting):

* [Please read before posting](https://old.reddit.com/r/PrintedCircuitBoard/comments/zj6ac8/please_read_before_posting_especially_if_using_a/) (rules 0–8)
* [Before you request a review, please fix these](https://old.reddit.com/r/PrintedCircuitBoard/comments/1jwjhpe/before_you_request_a_review_please_fix_these/)

Status is for **this** review package, regenerated 2026-07-31 after the silkscreen,
renumbering and stitching-via work.

## Image rules (rule 8)

| Requirement | Status |
|---|---|
| Exported/screen-captured, never a photo of a monitor | ✅ all via `kicad-cli` |
| No cursor / editor UI / OS chrome in the image | ✅ nothing was screen-captured |
| Background grids disabled | ✅ plots have no grid |
| Common web file types, not huge | ✅ PNG + PDF, 0.1–3.6 MB |
| Not fuzzy — legible at full size | ✅ 300 dpi schematics, 2600 px 2D, 2400 px 3D |
| Schematic on a light background, no rotation | ✅ KiCad light theme, standard orientation |
| 2D PCB: readable silkscreen against copper/background | ✅ white silk, red/green/orange/blue copper, black voids |
| 2D PCB: no net names on traces, no pin numbers on pads | ✅ plots never emit them |
| 2D PCB: nothing that isn't in the gerbers | ✅ only `*.Cu`, `*.Silkscreen`, `Edge.Cuts` |
| 2D PCB: board outline and cutouts enabled | ✅ `Edge.Cuts` on every plot |
| Board dimensions along two sides (optional) | ⚠️ not burned into the images; stated in the post text instead |
| Copper without pours | ✅ added — `07`–`10` per board, zones emptied so routing is visible |
| 3D: same orientation as the 2D views | ✅ top render matches 2D top; bottom render matches the mirrored 2D bottom |
| 3D: straight-down plan view mandatory | ✅ orthographic, no tilt |
| 3D: skip if most models are missing | ✅ every footprint that renders has a model; the carrier's J12–J15 headers are bottom-mounted, so they appear in the bottom view only |
| Separate labelled link per image, not one project link | ➡️ do this when posting; `POST-*.md` lists the captions |

## Schematic conventions

| Requirement | Status |
|---|---|
| Board name, revision, date in the title block | ✅ all four sheets |
| Project name where there are multiple boards | ✅ "KrakenPad …" on each |
| Personal name/initials removed for a public post | ⚠️ silkscreen reads "Kraken Pad by SenPi" — a handle, not a legal name; your call |
| Text/lines/symbols don't touch; no lines through symbols | ⚠️ eyeball the dense panel sheet at full zoom before posting |
| Ground symbols point down, positive rails point up | ✅ |
| Pull-ups drawn above the signal, pull-downs below | ✅ spot-checked (QSPI_SS pull-up, FSR dividers) |
| Decoupling caps below the rail, next to their IC | ✅ |
| Standard symbols, not featureless boxes | ✅ buffers as triangles, transceiver/diodes/transistors correct; MCU as a rectangle is conventional |
| RefDes start at 1 with no numeric gaps | ✅ master and panel-single contiguous from 1; ✅ dual-panel uses the multi-sheet exception — carrier 2xx, brain 3xx, each contiguous |
| Capacitance / resistance / inductance next to every part | ✅ |
| Frequency next to the crystal | ✅ "12MHz ABM8-272-T3" |
| Voltage next to TVS/zener parts | ✅ SMAJ5.0A, USBLC6-2SC6 named |
| Colour next to every LED | ⚠️ D1 (debug LED) has no colour called out |
| Pole/throw next to every switch | ⚠️ SW3 termination switch isn't marked DPDT; the panel-ID DIP isn't marked 4P |
| Purpose text next to LEDs/buttons/switches | ✅ "BOOTSEL", "RS-485 Termination", "PANEL_ID" |
| Part number next to every IC/regulator/transistor | ✅ RP2040, THVD1429, SN74AHCT125, AMS1117-5.0, AP7361C-33ER, LM66200, W25Q32JV |
| Connector family + pitch next to connector symbols | ⚠️ master says "B2B-XH-A"; the panel's J3/J4/J6/J7 say only "FSR North/East/South/West" — add "JST PH 2.0 mm" etc. |
| Linear regulator subcircuits drawn in→out, left to right | ✅ power-management block reads left to right |
| RS-485 drawn in the conventional form | ✅ |

**RefDes gaps: closed 2026-07-31.** master and panel-single renumber contiguously
from 1; dual-panel was re-annotated into per-sheet blocks (carrier 2xx, brain 3xx)
with no duplicates across the two sheets.

## PCB conventions

| Requirement | Status |
|---|---|
| Board name / revision / **year** in silkscreen | ✅ name + rev + year on all four boards |
| Mounting holes placed | ✅ master H1–H4, carrier + single panel corner holes, brain H1/H2 |
| Wide traces for power, GND floods | ✅ 4-layer with GND planes |
| No high-current/high-speed routing under the crystal | ➡️ worth confirming under X1 on the panel and brain |
| RefDes not hidden under the component it names | ✅ spot-checked on all four 2D top plots |
| Pin-1 / polarity / orientation marks in silkscreen | ✅ visible on ICs, electrolytics and connectors |
| Helpful silkscreen text — connector purpose, switch function | ✅ this is a strong point of these boards |
| Connector family + pitch in silkscreen | ⚠️ purpose is labelled ("RS-485 IN", "FSR North"), family/pitch is not |
| Max voltage next to power inputs | ✅ "+12VDC In" / "+12VDC Out" |
| Silkscreen typos | ✅ both fixed — J4 reads FSR East, test points read RS-485 A/B |

The board-house order-number placeholder has been removed from every board, so
it no longer appears in the bottom images.
