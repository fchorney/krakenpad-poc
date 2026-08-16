# Stock harness parts index

Connectors, contacts and cable found in the **stock SMX pad** at the 2026-08-08
teardown. Descriptive — this is a record of what the pad contains, so it can be
reassembled or repaired, not a shopping list.

Parts **this project buys** are in `hardware/harness/PARTS.md`.

## Connector gender conventions — READ BEFORE ORDERING ANY HOUSING

**Three vendors appear in this pad and two of them use "receptacle" to mean
opposite contact genders.** Getting this backwards does not corrupt the wiring
record (see below) but it does mean ordering the wrong housings *and* the wrong
crimps. A full audit on 2026-08-08 found most of the JST YL parts labelled
inverted.

| Convention | "Receptacle" holds | "Plug" holds |
|---|---|---|
| **JST wire-to-wire** (YL, VL, SM) | **PIN — male** (`SYM`/`SVM`) | **SOCKET — female** (`SYF`/`SVF`/`SHF`) |
| **Molex Mini-Fit Jr.** | **SOCKET — female** (5556) | **PIN — male** (5558) |
| **IEC 60320 mains** | *(socket/connector)* female | male |

JST states it verbatim in the housing tables of `eYL.pdf`, `eSM.pdf` and
`eVL2.pdf`: **"Receptacle housing (for pin contact) / Plug housing (for socket
contact)."** So the JST part suffix reads:

- `YLR` / `VLR` / `SMR` → **R**eceptacle → **male pins**
- `YLP` / `VLP` / `SMP` → **P**lug → **female sockets**

This is the reverse of the intuitive reading, and the intuitive reading is what
got recorded first for most of the 12 V side.

**Why they disagree — the mental model that makes both make sense:**

> **JST names by which housing enters the other. Molex names by the contact.**

JST's *plug* is the shell that inserts into the *receptacle* shell — and that
inserting shell happens to carry the female sockets. Molex ignores shell
geometry and names the half by what its crimps are, so its *receptacle* is the
female one. Neither vendor is inconsistent with itself; they are just answering
different questions with the same two words. Every part in this pad fits this
model.

**The bench test that settles it:** look into the mating face. Male blades
sticking out → JST **receptacle** (`xxR`). Holes → JST **plug** (`xxP`). Reverse
that reading for Molex — holes → Molex **receptacle**.

**"It plugs in, so it's the plug" does not work**, and it is what produced the
errors here in both directions. Both halves of a mated pair plug into each
other, and on Molex wire-to-board it is specifically the *receptacle* that plugs
onto the board header.

### Why this did not corrupt the wiring record

JST draws receptacle and plug housings as **mirror images** (`eYL.pdf` p.2,
"Contact position location numbers": the 4-circuit receptacle reads `4|2 / 3|1`,
the plug `2|4 / 1|3`). That mirroring exists exactly so that **pin 1 mates
pin 1** across a connection. Two consequences:

1. **Across a mate, pin N always meets pin N** — so every pinout recorded here
   stayed self-consistent regardless of which half was mislabelled.
2. **Reading numbers off a part in your hand still depends on knowing its type**,
   because of that same mirroring: counting left-to-right on a plug is counting
   right-to-left on a receptacle. This is why the mains 3-way's pins 1 and 3 have
   now moved twice (`ac-input.yml`).

Which is the standing reason every harness file here says **wire by color, not
by pin number**. JST's datasheets say position numbers are stamped into the
housings — **the JST housings in this pad are not marked** (checked 2026-08-08).
**The Molex 39014041 IS**, which makes it the only connector in the pad whose
pin numbering is directly observed rather than derived. Everything else was
re-read by physical position in the 2026-08-08 pinout pass; see `MEASUREMENTS.md`.

### Wire-to-board is a separate vocabulary

JST **XH** and **PH** (used on our own boards) are wire-to-board and carry no
plug/receptacle language at all — it is "housing" (`XHP-2`, `PHR-2`, female
socket contacts) mating a "shrouded header" (`B2B-XH-A`, `B2B-PH-K-S`, male
pins). Nothing to get backwards there.

The stock pad's **KF2510s** at the MCU are the same story: a *crimp housing*
with female contacts mating a *pin header*. No plug/receptacle gender question
applies to them either.

One habit worth copying from the stock build: **every position in those housings
has a contact fitted, wired or not.** The unwired ones exist so the housing grips
the header evenly. An unwired position is not an empty cavity.


## AC mains side (stock, documented not redesigned)

Nothing here is bought for this project unless something breaks during
reassembly. Recorded so the pad can be put back together correctly.

| Part | Notes |
|------|-------|
| Wall cord | ordinary PC power cord (C13 to mains plug), purchased |
| YD06 EMI filter | 125/250 VAC, 6 A; **C14 inlet**, **6.3 mm** output tabs |
| Locking female spade terminals ×3 | for **6.3 mm** tabs, 16 AWG |
| JST **YLP-03V** 3-way (plug, female sockets) | brown / **green centre** / blue; appears unmated. Recorded as YLP-03V, "corrected" to YLR-03V, and reverted to **YLP-03V** on 2026-08-08 — the original call was right. See `WIRE_COLORS.md` |
| XUANHUA XC13-X | locking IEC 60320 **C13**, 13 A 250 V, UL file E257089 |
| **YU1208** 12 V PSU | 102 W; in AC 100–240 V 1.8 A 50/60 Hz; out **DC 12 V 8.5 A**; C14 inlet; outer −, inner + |
| Inline joint under heatshrink | unidentified, not opened; passes 12 V/GND |
| JST **YLR-02V** 2-way (receptacle, **male pins**) | PSU output. **Pin 1 = GND (black), pin 2 = 12 V (red)** — re-read 2026-08-08, reversing an earlier derived reading. Was also recorded YLP-02V — gender corrected same day |
| JST **YLP-02V** 2-way ×2 (plug, female sockets) | star point "A" and "B"; both **pin 1 = GND (black), pin 2 = 12 V (yellow)**, 16 AWG — identical parts, label them. Pin order re-read and gender corrected 2026-08-08 |
| Fork terminals ×2 | **4.0 mm ID / 6.5 mm OD**; the 12 V and GND star points |
| JST **SMR-03V-B** (receptacle, **male pins**) | **pin 1 = GND (black), pin 2 = DATA (white), pin 3 = 12 V (yellow)** — underglow connection point. **All three conductors 18 AWG.** Gender and pinout both confirmed |
| JST **YLR-01V** 1-way (receptacle, **male pin**) | underglow DATA in; the stock MCU's output. Was recorded YLP-01V — corrected 2026-08-08. Its mate (MCU side) is not yet documented and must be a YLP-01V |
| JST **YLR-02V** 2-way (2nd, receptacle) | on the barrel-input cable; mates star-point plug B. **Pin 1 = GND (black), pin 2 = 12 V (yellow)**, 16 AWG — re-read 2026-08-08. Same part *and* same pinout as the PSU output above: both are 12 V *sources* presenting the same face to the star point |
| Barrel **socket** | ~5 mm bore / ~10 mm body (approx — hot-glued in, calipers don't fit), outer = GND, inner = 12 V; external cabinet 12 V input |
| **Daygreen B15-1224-05** | 75 W DC-DC; in **12–24 V**, out **5 V 15 A max**; screw terminals both sides. The pad's 12V/5V boundary |
| Fork terminals, 5 V side | same **4.0 mm ID / 6.5 mm OD** as the 12 V side |
| JST **SMR-02V-B** (receptacle, **male pins**) | 5 V branch; pin 1 = 5 V (red), pin 2 = GND (black), **18 AWG**. Gender and pinout both confirmed; its mate is not yet documented and must be an SMP-02V-BC |
| JST **VLR-04V** 4-way (receptacle, **male pins**) | **VL series, not YL** — the heavier power family (accepts 22–12 AWG; the pad's conductors are **18 AWG**). Pins: 1 green GND, 2 white 5 V, 3 red 5 V, 4 black GND. Gender and pinout both confirmed |
| JST **VLP-04V** 4-way (plug, female sockets) | mates the VLR-04V; fans out to three branches. Gender was already correct |
| JST **SMP-02V-BC** (plug, **female sockets**) | mates the 5 V SMR-02V-B; pin 1 = 5 V (red), pin 2 = GND (black), **22 AWG**. The MCU end of the 5 V branch |
| JST **YLP-01V** 1-way (plug, **female socket**) | mates the underglow YLR-01V; white 22 AWG. The MCU end of the underglow DATA line |
| **KF2510 or clone**, 14-circuit crimp housing | 2.54 mm pitch, female crimp contacts, pushed straight onto the Micro's own header pins — no adapter board. Marked only "2510". Covers the analog row from D13. **Only 3 of 14 positions carry a wire:** 5 = DATA (A1), 12 = 5 V, 14 = GND |
| **KF2510 or clone**, 17-circuit crimp housing | Same family, covers the Micro's whole digital row from D12. **11 of 17 positions carry a wire:** 3–11 = the nine panel signal lines (D10 down to D2), 12 = data GND, 15 = data TX |
| JST **YLR-09V** (receptacle, **male pins**) | 3×3 9-circuit, MCU side. All nine panel signal lines. Pins 1–9 = yellow, orange, red, brown, blue, green, black, white, grey = panels 2,1,0,5,4,3,8,7,6 per the stock SMX color map |
| JST **YLP-09V** (plug, **female sockets**) | panel side, mates the above pin-for-pin. Nine **18 AWG** home runs, each ending in a **crimp pin** into a panel terminal block. Note the gauge steps *up* from the 22 AWG used MCU-side |
| **2-position terminal block** ×9 | one per panel, takes the signal-line crimp pin. **Signal position used, GND position empty** — the return rides the shared power ground. Positions are marked on the block |
| **FSR leads** (stock, reused) | red/black **30 AWG**, only **~10 cm** long, ending in a JST **PHR-2**. The short lead is what forces FSR connectors onto the panel's cardinal edges |
| **RJ-12 receptacle**, marked 623K | 6-position modular jack, the panel data bus out. Only 2 conductors used: **pin 3 = DATA** (red, from D1/TX), **pin 4 = GND** (green) — the **centre pair**, so 6P2C/6P4C/6P6C all carry it. **Cables must be STRAIGHT-THROUGH**, not the usual reversed telephone type, which would swap DATA and GND |
| **RJ-12 patch leads** ×9 | the data chain: **1 × 60 cm** (MCU to panel 0) + **8 × 45 cm** (panel to panel). **4.2 m total. 6P4C**, of which only the centre pair does anything. Buy **6P4C or better, straight-through**; gauge is immaterial |
| **RJ-12 jacks on each panel** ×18 | two per panel, in and out. Wired **pin-for-pin, not mirrored** (deduced — a mirror anywhere would swap DATA/GND for everything downstream) |
| **Arduino Micro** | the stock MCU, plain 0.1 in male headers on both rows. Powered from the pad's 5 V rail; **A1 drives the underglow data line**. The stock design uses **none** of the other analog inputs — A0 and A2–A5 are unconnected |
| **Molex 39014041** ×9 (receptacle, **female sockets**) | **Mini-Fit Jr., 4-circuit, single row.** 3 on the column fan-out + 6 on the panel-to-panel jumpers (2 per column), so **12 housings** in total counting both ends of each jumper. **Housing colour is a build convention: natural/clear = plugs into a panel INPUT, RED = plugs into a panel OUTPUT.** Same part either way. Pins: 1 red 5 V, 2 black GND, 3 green GND, 4 white 5 V, all **18 AWG**. **This housing carries moulded pin numbers** — the only observed pin record in the pad. Holes confirm it is a Molex receptacle taking 5556-family female crimps, so the part number is right. Note this is the **opposite** gender-sense to every JST "receptacle" above |
| Chassis ground ring terminal | **4.0 mm ID / 6.5 mm OD** (M4), 18 AWG |
| Jacketed 16 AWG 3-conductor | black L / white N / green GND |
| 18 AWG hookup wire | brown, blue, green, green-yellow — branch pigtails |

### JST families in the stock pad

Three of them, and two are one letter apart:

| Series | Where | Notes |
|--------|-------|-------|
| **YL** | 12 V side — `YLR-03V`, `YLP-02V`/`YLR-02V`, `YLP-01V` | the lighter family, 16–18 AWG |
| **VL** | 5 V side — `VLR-04V` | JST's **heavier power** series, visibly larger; accepts 22–12 AWG though the pad uses 18 AWG |
| **SM** | branch connectors — SM 3P (underglow), SM 2P (5 V) | different circuit counts prevent cross-plugging |

`YLR-04V` and `VLR-04V` differ by one letter and are easy to confuse by eye —
the 4-way on the 5 V side is the **V**L part.


### JST YL current ratings (from `eYL.pdf` p.1, transcribed 2026-08-16)

The rating is **a function of both wire gauge and circuit count** — a single
"10 A" headline number is not usable. Current in amps:

| Circuits | #16 | #18 | **#20** | #22 | #24 | #26 |
|---|---|---|---|---|---|---|
| 1 | 10 | 7 | **5** | 4 | 3 | 2 |
| **2** | **10** | **7** | **5** | **4** | **3** | **2** |
| 3 | 9 | 6 | 4 | 4 | 3 | 2 |
| 4 | 9 | 6 | 4 | 4 | 3 | 2 |
| 6 | 8 | 5 | 3 | 3 | 2 | 2 |
| 8 | 7 | 4 | 3 | 3 | 2 | 2 |
| 9 | 6 | 4 | 3 | 3 | 2 | 2 |

Other YL specs from the same page:

- Series max **10 A AC/DC, but 7 A when retainers are mounted**. Retainers
  cannot be used with AWG #16 wire, or where insulation OD exceeds 2.7 mm.
- Voltage 300 V AC/DC. Applicable wire **AWG #26 to #16** (0.13–1.25 mm²).
- Contact resistance **7 mΩ max initial, 10 mΩ max after environmental tests**.
- Temperature range −25 to +90 °C, **including the connector's own temperature
  rise under current** — so the table already assumes self-heating, and a warm
  enclosed pad eats into that margin. Derate rather than running at the number.
- Two contact wire ranges: `SYM/SYF-01T-P0.5A` for #26–#20 and
  `SYM/SYF-41T-P0.5A` for #20–#16. **#20 sits on the boundary and could be
  either** — the rating is set by wire size regardless.

**Why this matters here:** the stock PSU tail is 2 circuits at **#20**, so it is
rated **5.0 A** against our 6.34 A worst case. That is what decided the tail
removal in `12v-trunk.yml`. The VL ratings for comparison (`eVL-WW.pdf`, 6.2 mm,
#22–#12): 2 circuits = 20 A @#12, 15 A @#14, 10 A @#16, 8 A @#18.

