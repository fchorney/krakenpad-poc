# Teardown Capture Sheet — 2026-08-08

Raw bench measurements from the full pad teardown. Fill this in first, in
whatever units you measured; then transcribe into the `.yml` files (which want
metres) and regenerate with `./gen.sh`.

Rule of thumb for lengths: measure **connector face to connector face**, not
including the housings, and note separately whether the stock routing has
noticeable slack you could shorten.

> ### ⚠ UNITS: every stock length on this page is CENTIMETRES
>
> All lengths recorded during the 2026-08-08 teardown were **reported as
> millimetres and meant as centimetres**. The whole set was corrected ×10 the
> same day — the `.yml` files, this page, `WIRE_COLORS.md` and `README.md`.
>
> This was caught late, and only because the nine panel home runs summed to an
> impossible 0.93 m of wire. **Two earlier notes had actively defended the wrong
> figures** — `ac-input.yml` insisted its cable was "genuinely 45 mm, not a
> units slip" and built a rationale around it, and `5v-distribution.yml`
> described "very short ~7 mm tails" that would have been too short to strip.
> Neither survived.
>
> **Sanity-check any new length against something physical before recording it.**
> A 7 mm pigtail, a 10 mm cable or a 60 mm run across a ~900 mm pad should each
> have been caught on sight. The `.yml` files store metres, as WireViz expects;
> the conversion from a cm bench reading is ÷100.

## Confirmed at teardown

- **Underglow connection point: JST SMR-03V-B receptacle** (male pins), part of the 12 V
  distribution harness (`12v-distribution.yml`).
  This supersedes `docs/UNDERGLOW.md`'s "crimped into a 12-pin Dupont-style
  housing, no intermediate connector to reuse" — there *is* a reusable
  connector, and the splice question is closed.
  - [x] **Pin order RESOLVED — and it is the reverse of what was assumed:**
        **pin 1 = GND (black), pin 2 = DATA (white), pin 3 = 12 V (yellow).**
        The earlier working assumption was 12V/DATA/GND. **All three conductors
        are 18 AWG** (re-measured 2026-08-08; power was recorded as 16 AWG), so
        gauge does not distinguish data from power here — color does.
  - [x] Underglow **DATA arrives on a JST YLR-01V**, a single-conductor
        connector — the stock MCU's output. This is the one point the
        replacement master must interface with, and it is a connector, not a
        splice. See the supersession note at the top of `underglow.yml`.

- **AC mains input** (`ac-input.yml`): wall cord into a **YD06 EMI filter**
  (125/250 VAC, 6 A). Filtered L/N/GND out onto a jacketed **16 AWG 3-conductor**
  cable, each conductor double-terminated: a locking female spade onto the
  filter tab, plus a splice to a branch pigtail. Branches go to a **JST YLP-03V**
  (brown / **green centre** / blue — ground in the middle) and, on ground
  only, to a **chassis ring terminal**. Far end is a locking **IEC C13**.
  The branch pigtails are **separate 18 AWG conductors**, not part of the 16 AWG
  jacket. Colors are recorded in `WIRE_COLORS.md` and do **not** match across the
  splices — L is black in the cable but brown in the pigtail, N is white then
  blue. The two ground branches are deliberately distinguishable: plain green to
  the YL 3-way, green/yellow to the chassis ring.

  Confirmed: filter inlet is an **IEC C14** (ordinary PC power cord); filter
  output tabs are **6.3 mm**; the far-end connector is a locking **IEC C13**,
  XUANHUA **XC13-X**, 13 A 250 V, UL file E257089; ring terminal is **4.0 mm ID /
  6.5 mm OD** (M4). The **YL 3-way appears to mate with nothing** — likely a
  leftover from an earlier revision, but it carries live mains, so decide at
  reassembly whether to remove or insulate it. It was first read as a YLP-03V,
  "corrected" to a YLR-03V, then **reverted to `YLP-03V`** by the 2026-08-08
  gender audit — JST's *receptacle* is the male-pin half, the reverse of the
  intuitive reading that drove the first correction. Brown/L is back on pin 1.
  The *colors* were unaffected throughout and are the record to trust.

  The 16 AWG cable is **45 cm** (43 cm measured, built to 45), running from the
  filter to the PSU's C14 inlet. Everything from the filter onward is inside the
  pad; only the cord is external.

  - [ ] *(optional)* Photo of the chassis bonding point. It's near where mains
        enters, and obvious with the filter in front of you — not worth a
        special trip.

- **12V PSU** (`psu-12v.yml`): the C13 plugs into a **YU1208** 102 W switching
  supply — in AC 100–240 V 1.8 A 50/60 Hz, out **DC 12 V 8.5 A**, polarity marked
  outer negative / inner positive. Its captive 18 AWG 2-conductor output cable is
  long and stored bunched and twist-tied; length deliberately not measured. It
  passes through an **unidentified joint under heatshrink** (not opened) to a
  30 cm 20 AWG jacketed tail ending in a **JST YLR-02V** (receptacle, male
  pins), **pin 1 = GND (black), pin 2 = 12 V (red)** — both the part number and
  the pin order were corrected on 2026-08-08.
  Its captive cable is **two black 18 AWG conductors, one with a moulded ridge**
  — the only place in the pad where color does not identify a conductor.
  - [x] **Which of the two is +12V — STILL NOT ESTABLISHED, and now it matters
        more.** Polarity is unambiguous at the YL 2-way (red = 12 V,
        black = GND) and at the barrel (the supply's label reads
        centre-positive), so nothing in the *stock* pad needs the answer.
        **But the "sealed joint" reasoning that closed this is obsolete:** the
        heatshrink came off 2026-08-16 and the joint is a plain 5.5 × 2.5 DC
        barrel pair, not a splice. Anyone cutting that captive cable must meter
        the barrel first — it is the only polarity reference on the supply side.
  - [x] **PSU nameplate: DC 12 V, 8.5 A, 102 W (YU1208).** Recorded as an
        observation. The **Gen 4 / Gen 5 nameplate discrepancy** — the manual
        gives Gen 4 as 12 V 9 A and Gen 5 as 12 V 15 A, and this matches
        neither — is left unresolved on purpose.
        Whether 8.5 A is adequate for *this project*, and whether the supply is
        kept, are design questions and live in `hardware/harness/README.md`
        → "Power budget", not here.

## Pinout re-verification pass — COMPLETE 2026-08-08

Triggered by the connector-gender audit: some pin numbers had been *derived*
from an assumed gender rather than read off the part, and receptacle/plug
housings mirror, so a wrong gender call mirrors the numbering.

**Every connector in the stock record was re-read against the physical part.**
Results are transcribed into the `.yml` files; this section is the record of
what moved.

### What the pass found

**Pin order reversed on all four YL 2-ways** — GND is pin 1, +12V is pin 2:

| Connector | File | Was | Now |
|---|---|---|---|
| `YL2` PSU output | `psu-12v` | 1=12V red, 2=GND black | **1=GND black, 2=12V red** |
| `YLP_A` star point A | `12v-distribution` | 1=12V yellow, 2=GND black | **1=GND black, 2=12V yellow** |
| `YLP_B` star point B | `12v-distribution` | 1=12V yellow, 2=GND black | **1=GND black, 2=12V yellow** |
| `YLR_BARREL` barrel cable | `external-12v-input` | 1=12V yellow, 2=GND black | **1=GND black, 2=12V yellow** |

**Conductor gauge corrected on the whole 5 V distribution harness and the
SM 3P branch** — both were recorded heavier than they are:

| Conductors | File | Was | Now |
|---|---|---|---|
| SM 3P power (12V + GND) | `12v-distribution` | 16 AWG | **18 AWG** |
| SM 2P pair | `5v-distribution` | 12 AWG | **18 AWG** |
| VL 4-way, all four | `5v-distribution` | 12 AWG | **18 AWG** |

**Confirmed unchanged** — these were right the first time:

- `YLP-03V` mains 3-way: 1 = brown L, 2 = green GND, 3 = blue N. Settles a
  number that had moved twice.
- `SM3` underglow: 1 = GND black, 2 = DATA white, 3 = 12V yellow
- `SM2` 5 V branch: 1 = 5V red, 2 = GND black
- `FOURWAY` / `VLP` VL 4-ways: 1 = green GND, 2 = white 5V, 3 = red 5V,
  4 = black GND — and the `VLP` half is now *read* rather than inferred from
  its mate, retiring that caveat
- `MOLEX_1/2/3`: 1 = red 5V, 2 = black GND, 3 = green GND, 4 = white 5V
- All IEC mains parts, fork/ring lugs, the barrel socket, the DC-DC screw
  terminals and the concealed joint — nothing numbered to re-read

### Two facts worth keeping

**The Molex 39014041 is the only connector in the pad with moulded pin
numbers.** Its pinout is therefore *observed*; every JST housing here is
unmarked, so those numbers are read by physical position instead. JST's
datasheets do say position numbers are stamped — these particular housings
are not.

**Nothing physical was ever mis-wired by any of this.** Colors were read
straight off the conductors and no gender or numbering error could touch them,
and pin 1 mates pin 1 across every JST pair, so the harness stayed
self-consistent throughout. The errors were confined to what to *order* and
what to *write down*.

### Consistency checks — all pass

- Every mated pair agrees pin-for-pin: `YL2`↔`YLP_A`, `YLP_B`↔`YLR_BARREL`,
  `FOURWAY`↔`VLP`.
- `YLP_A` and `YLP_B` are the same part, identically wired — still true.
- `YL2` and `YLR_BARREL` are the same part with the same pinout, which is what
  you would expect of the two 12 V *sources* landing on the star point.
- The VL-to-Molex order flip survives: VL runs GND,5V,5V,GND and the Molex runs
  5V,GND,GND,5V. Only the color-to-pin mapping differs, and it is unchanged.
- The 18 AWG finding resolved a contradiction already present in the record —
  `5v-distribution.yml` said 12 AWG while `5v-columns.yml`, downstream of the
  same connector, already said 18 AWG.

### Gender predictions — BOTH CONFIRMED 2026-08-08

The audit forecast two mates it had not yet seen, purely from the JST rule. The
MCU harness (`mcu-interface.yml`) turned both up and both were right:

- Underglow DATA 1-way, MCU side → predicted **YLP-01V**, found **YLP-01V**
- 5 V branch, MCU side → predicted **SMP-02V-BC**, found **SMP-02V-BC**

Good evidence the gender rule in `PARTS.md` is now stated correctly.
- Whether the stock 5 V panel load crossing the VL 4-way is adequate.
  **Figure corrected 2026-08-14:** this said 8.1 A / ~4 A per conductor, but
  8.1 A was our *12 V* budget wrongly applied to a 5 V connector. Stock is
  225 × ~58.5 mA ≈ **13.2 A, so ~6.6 A per conductor** — against JST's 7 A
  rating for a 4-circuit VL at #18. It sits at ~94% of rating at full white,
  which is why both rails are doubled. Moot for us: the whole 5 V chain goes.

## MCU interface — `mcu-interface.yml`, recorded 2026-08-08

**This closed "where does the 5 V SM 2P go": it goes to the MCU.**

- The 5 V branch (`SMP-02V-BC`, pin 1 = 5 V red, pin 2 = GND black) and the
  underglow DATA return (`YLP-01V`, white) share **one ~25 cm 22 AWG bundle**
  landing on a **14-pin 2510 / KF2510 crimp housing**.
- On the 2510: **pin 5 = DATA, pin 12 = 5 V, pin 14 = GND.** Pin 1 lands on the
  Arduino's **D13**.
- At the Arduino Micro: DATA → **A1**, 5 V → 5 V in, GND → GND. So the board is
  powered from the pad's 5 V rail and **A1 drives the underglow**.
- **The underglow data chain is now complete end to end:** Arduino A1 → 2510
  pin 5 → YLP-01V → YLR-01V → SM 3P pin 2 → strips.

**Only 3 of the 14 positions are populated.** The other eleven are *empty* —
no crimp, no wire. The wide housing provides registration and retention, not
conduction. Position 1 sitting on D13 is a mating/alignment reference, not a
connection.

**Position map verified against the official Arduino Micro pinout**
(`Pinout-Micro_latest.pdf`). Counting along that header row from D13, all four
teardown readings land exactly:

| Housing position | Micro pin | Wired? |
|---|---|---|
| 1 | D13 | alignment reference only |
| 2 | +3V3 | empty |
| 3 | AREF | empty |
| 4 | A0 | empty |
| **5** | **A1** | **white — underglow DATA** |
| 6–9 | A2, A3, A4, A5 | empty |
| 10–11 | NC, NC | empty (physically NC on the board) |
| **12** | **+5V** | **red — 5 V in** |
| 13 | RESET | empty |
| **14** | **GND** | **black — GND** |

The housing covers 14 of the row's 17 pins; VIN, CIPO/D14 and SCK/D15 sit past
its end, uncovered.

**So the stock design uses none of the Micro's analog inputs.** A0 and A2–A5 are
unconnected; A1 is used but as a digital *output* driving the LED data line.
AREF, 3V3 and RESET are unconnected too.

**No adapter board exists** — the Micro carries plain 0.1 in male headers on
both rows and the KF2510 housings push straight onto them.

**Contacts are fitted in every position**, wired or not — the unwired ones grip
the header. An unwired position is not an empty cavity. Same on both KF2510s.

## MCU panel I/O — `mcu-panel-io.yml`, recorded 2026-08-08

The Micro's digital row, 17-pin KF2510 covering it from D12. **This is the whole
stock panel interface.**

- **Nine per-panel signal lines**, positions 3–11 = D10 down to D2, 22 AWG,
  ~30 cm, onto a **JST YLR-09V**.
- **Panel data bus**: position 12 = GND (green) and position 15 = D1/TX (red),
  26 AWG, ~15 cm, onto an **RJ-12 jack marked 623K**.
- Unwired: 1 (D12), 2 (D11), 13 (RESET), 14 (D0/RX), 16 (D17/SS), 17 (D16/COPI).

**Position map verified** against `Pinout-Micro_latest.pdf` — position 1 = D12,
3–11 = D10…D2, 12 = GND, then a two-position gap (RESET, D0/RX), then 15 = D1/TX.
Every landmark lands exactly.

**The nine colors decode to the stock SMX panel map** already in `docs/BOM.md`,
so each wire names its panel — an independent confirmation of that map from the
MCU end. **`Dn` drives panel `n−2`.** Full table in `WIRE_COLORS.md`.

**The bus is TX-only.** D0/RX is covered by the housing and unconnected, as are
SS and COPI — so the panel bus is neither SPI nor bidirectional UART, and
nothing returns to the MCU on it. Consistent with the reverse-engineered stock
protocol, where sensor data comes back on the per-panel signal wires instead.

Open on this harness:

- [x] **Which RJ-12 positions** — **pin 3 = DATA (red), pin 4 = GND (green)**.
      Read with moderate confidence; worth one confirming look.
- [x] **Whether the cable is 6P6C, 6P4C or 6P2C — no longer load-bearing.**
      Positions 3 and 4 are the **centre pair** ("line 1"), which every modular
      cable populates including 6P2C. Any of the three carries this bus.
- [x] **Cable polarity — STRAIGHT-THROUGH.** Ordinary flat modular cable is
      *reversed* (1↔6, 2↔5, 3↔4), which on a 2-wire centre-pair bus would swap
      DATA and GND outright. Evidence is functional rather than visual: the
      cables were replaced with straight-through ones in the past and the pad
      worked. **Buy straight-through; a random telephone cable will not do.**
- [x] **Data pair gauge — called as 26 AWG.** The conductors are too covered to
      read markings, so this is a judgement from thickness, between 24 and 26.
      Nothing at this length and current depends on which.

## Panel signal lines — `panel-signal-lines.yml`, recorded 2026-08-08

The YLP-09V and its nine home runs. **All 18 AWG**, heavy jacket — the gauge
steps *up* from the 22 AWG the same signals use between the YLR-09V and the MCU.
Each ends in a **crimp pin** landing in a terminal block on its panel.

Lengths as reported, mapped through the color code:

| Panel | Position | Color | YLP-09V pin | Length |
|-------|----------|-------|-------------|--------|
| 0 | UL | red | 3 | 90 |
| 1 | U | orange | 2 | **60 — shortest** |
| 2 | UR | yellow | 1 | 70 |
| 3 | L | green | 6 | 120 |
| 4 | C | blue | 5 | 90 |
| 5 | R | brown | 4 | 100 |
| 6 | DL | grey | 9 | **150 — longest** |
| 7 | D | white | 8 | 120 |
| 8 | DR | black | 7 | 130 |

Laid out as the pad's 3×3 grid:

```
 UL  90    U  60    UR  70
  L 120    C  90     R 100
 DL 150    D 120    DR 130
```

**These fit a formula exactly:** `60 + 30 per row down + column offset`, where
the offset is 0 centre / 10 right / 30 left. All nine, no exceptions — so the
measurements are internally consistent and carefully taken. The MCU sits nearest
panel 1 (top centre) and slightly right of centre.

- [x] **Unit confirmed: CENTIMETRES.** **Total 9.3 m of 18 AWG** for the nine
      home runs, before slack. Caught by two checks — the 30-unit row step
      against a ~300 mm panel pitch, and the 930 total being an impossible
      0.93 m of wire as mm versus a sensible 9.3 m as cm.
- [x] **Data pair gauge — 26 AWG.** Markings unreadable; judged from thickness.
- [x] **The YLR-09V's mate is a `YLP-09V`**, and both the signal lines and the
      RJ-12 are now documented on the panel side.

## Panel power chain — `panel-power-chain.yml`, recorded 2026-08-08

Power reaches each column's first panel from the fan-out Molex, passes
**straight through the panel PCB** to an output header on the opposite side, and
jumps to the next panel. Three columns of three, **two jumpers each, six per
pad**, all 4-conductor 18 AWG at **60 cm**.

Columns (repo 0–8 numbering): **0-3-6**, **1-4-7**, **2-5-8** — which matches the
column topology `CLAUDE.md` always assumed. Only the *voltage* (5 V, not 12 V)
and the *source* (converter, not PSU lugs) differ.

**The last panel of each column — 6, 7 and 8 — has an unmated power output.**
Panels are interchangeable; being last is positional, not a different board.

| End | Housing colour | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|-----|---------------|-------|-------|-------|-------|
| Panel **INPUT** | natural/clear | red | black | green | white |
| Panel **OUTPUT** | **red** | white | green | black | red |

- **Housing colour is a build convention** — clear = input, red = output. Same
  Molex 39014041 either way.
- **The conductor order reverses** between the two ends of a jumper, because the
  panel's two headers are one part facing opposite ways.
- **The net order does not** — `5V, GND, GND, 5V` is a palindrome. Only which
  coloured conductor sits where changes.

**Wire needed:** 6 × 60 cm = **3.6 m** of 4-conductor 18 AWG for the jumpers,
plus 3 × 60 cm = 1.8 m for the column feeds. **5.4 m total.**

> **Panel numbering:** this page uses the repo's **0–8**. Teardown notes were
> spoken in 1–9 — subtract one. ("Panels 7, 8, 9 have an unconnected output"
> = repo panels 6, 7, 8.)

## Panel data chain — `panel-data-chain.yml`, recorded 2026-08-08

The RJ-12 bus daisy-chains through all nine panels. Each panel has **two RJ-12
jacks**, in and out, and passes the bus along.

**Chain order — a serpentine, confirmed at teardown:**

```
MCU → 0(UL) → 3(L) → 6(DL) → 7(D) → 4(C) → 1(U) → 2(UR) → 5(R) → 8(DR)
```

**This matches `CLAUDE.md` exactly**, where it came from a logic capture of the
stock protocol rather than from the physical pad. Two independent sources, same
answer.

**Nine cables: 1 × 60 cm** (MCU to panel 0, the only long run) **+ 8 × 45 cm.**
**4.2 m total.** Conductor count, colours and gauge were not recorded — only the
centre pair carries anything, so any modular lead works **provided it is
straight-through**.

**Panel 8's output jack is empty**, with no termination of any kind. Mirrors the
power chain, where 6, 7 and 8 all have unmated outputs.

### Power and data use the same columns, traversed differently

The serpentine *is* the three power columns, walked alternately down and up:
left down (0,3,6), centre up (7,4,1), right down (2,5,8). Power feeds each
column independently from the fan-out; data snakes through all three in one
continuous run.

Open on this harness:

- [x] **RJ-12 leads are 6P4C** — four conductors. Positions 2, 3, 4, 5 are
      populated, so the centre pair is covered and 2 and 5 go nowhere. **Gauge
      is immaterial** on a signal pair over a short run. When buying
      replacements: **6P4C or better, and straight-through.**
- [x] **The panel's two jacks are wired pin-for-pin — CONFIRMED**, upgrading
      what had been a deduction. This is what lets straight-through leads work
      end to end.

## Panel signal terminal, FSR and LEDs — recorded 2026-08-08

**Signal-line terminal block.** Each panel takes its signal line on a
**2-position terminal block: one signal, one GND — and the GND position is
unused.** So each panel's signal returns through the shared power ground rather
than a dedicated conductor. Which position is which is **marked on the block**,
so it needs no record here.

> Worth contrasting with this project's own design, which deliberately adopted a
> **dedicated twisted-pair return** on both positions of its INT terminal
> (carrier J214, `CLAUDE.md`). The stock pad does not do that — so our INT wiring
> is a change from stock, not a copy of it.

**FSR leads.** Red and black, **30 AWG** — extremely fine — and **~10 cm long**.
No extension, no inline joint: the sensor reaches its connector directly or not
at all.

> **This is the constraint that fixes FSR connector placement.** A 10 cm pigtail
> is why the connectors sit on the *cardinal edges* of the panel PCB — carrier
> J201 (W), J202 (S), J203 (E), J206 (N). A centre-mounted connector would be out
> of reach. Noted in `fsr-panel.yml`.

**LED wiring: there isn't any.** The 25-LED string is **entirely PCB traces** on
the stock panel board — no internal harness, nothing to document, nothing to
rebuild. The only LED-related wiring in the whole pad is the underglow.

## Stock inventory

Count and photograph everything before anything gets cut up.

| Item | Qty found | Connector(s) each end | Notes |
|------|-----------|----------------------|-------|
| AC wall cord | | 3-prong / filter inlet | purchased, user-replaceable |
| YD06 EMI filter | | | 125/250 VAC 6A |
| AC 16 AWG cable + branches | | spades / locking C13 | + YL 3-way and ring branches |
| YU1208 12V PSU | | C14 in / captive DC out | 102W, 12V 8.5A |
| 12V column harness (PSU to first panel) | | | 3 expected, one per column |
| 12V panel-to-panel jumper | | | 6 expected |
| RS-485 / data segment (MCU to panel 0) | | | 1 |
| RS-485 / data segment (panel to panel) | | | 8 |
| Sensor / INT cabling | | | stock topology differs from ours |
| FSR sensor + tail | | | 36 expected |
| Underglow strip pigtail | | JST SMP-03V-BC (plug, female sockets) | 1 |
| Underglow strip segments | | | left 17 + right 17 + back 10 chunks |
| PSU | 1 | C14 in / YL 2-way out | **YU1208, 12V 8.5A** — not the 15A Gen 5 figure |
| Anything unaccounted for | | | |

## Lengths to measure

### AC input — `ac-input.yml`

| Run | Length | Gauge | Color | Notes |
|-----|--------|-------|-------|-------|
| 16 AWG filter to C13 | **45 cm** | 16 | BK / WH / GN | 43 measured, build 45 |
| Splice to YL 3-way, L | ~15 cm | 18 | brown | separate conductor |
| Splice to YL 3-way, N | ~15 cm | 18 | blue | separate conductor |
| Splice to YL 3-way, GND | ~15 cm | 18 | green | separate conductor |
| Splice to chassis ring terminal | ~20 cm | 18 | green/yellow | ground only |
| Wall cord | n/a | n/a | n/a | ordinary PC power cord, not measured by choice |

### 12V columns (PSU to first panel) — `power-column.yml` `W_PSU`

| Column | First panel | Length | Slack? |
|--------|-------------|--------|--------|
| Left | 0 (UL) | | |
| Center | 1 (U) | | |
| Right | 2 (UR) | | |

### 12V panel-to-panel jumpers — `power-column.yml` `W_JUMP_A` / `W_JUMP_B`

| Segment | Length | Same as others? |
|---------|--------|-----------------|
| 0 to 3 | | |
| 3 to 6 | | |
| 1 to 4 | | |
| 4 to 7 | | |
| 2 to 5 | | |
| 5 to 8 | | |

### RS-485 chain segments — `rs485-chain.yml`

Serpentine order: master, 0(UL), 3(L), 6(DL), 7(D), 4(C), 1(U), 2(UR), 5(R), 8(DR).

| Segment | Length |
|---------|--------|
| master to 0 | |
| 0 to 3 | |
| 3 to 6 | |
| 6 to 7 | |
| 7 to 4 | |
| 4 to 1 | |
| 1 to 2 | |
| 2 to 5 | |
| 5 to 8 | |

### INT home runs — `int-home-run.yml`

All nine run individually back to the master, so all nine are different.
Stock has no equivalent, so these are **new builds** — measure the *route* you
intend to use, not an existing cable.

| Panel | Position | Length | Marker color |
|-------|----------|--------|--------------|
| 0 | UL | | Red |
| 1 | U | | |
| 2 | UR | | |
| 3 | L | | |
| 4 | C | | |
| 5 | R | | |
| 6 | DL | | |
| 7 | D | | |
| 8 | DR | | Black |

(Marker colors follow the stock SMX map in `docs/BOM.md`. Fill the middle rows
from that table.)

### FSR leads — `fsr-panel.yml`

| Edge | Length | Notes |
|------|--------|-------|
| North / East / South / West | **~10 cm each** | stock lead, 30 AWG red+black |

- [x] **All four are the same, ~10 cm** — it is the FSR's own moulded pigtail,
      not a made-up cable, so there is nothing to cut to length. **That 10 cm is
      what forces the connectors onto the cardinal edges.**

(Plug type is **not** an open question either — PHR-2 is confirmed against a real
lead and the mating header is sourced, LCSC C131337.)

### Underglow — `underglow.yml`

| Run | Length | Notes |
|-----|--------|-------|
| SM 3P to master | | depends on master mounting position |
| Master GND tie | | mandatory even without underglow |

- [x] **"PSU terminal screw size" is moot** — the stock PSU is a brick with a
      single JST YL 2-way output and no terminal block or ground stud at all.
      The 12 V star point is physically the DC-DC converter's input screws.
- [ ] **This whole table may not be needed.** The stock harness already feeds the
      strips their 12 V and GND, and the MCU only ever drove **one wire** into a
      `YLR-01V`. See the supersession note at the top of `underglow.yml` —
      resolve when our own harness design starts.

## Open questions raised by the teardown

### 1. How power reaches the three columns — ANSWERED, and it is 5V

**The stock pad feeds the panel columns at 5 V, not 12 V.** The chain is:

12 V star point (= the DC-DC converter's input screw terminals)
→ **Daygreen B15-1224-05**, 12 V→5 V 15 A
→ 5 V fork terminals
→ **JST VLR-04V / VLP-04V**
→ **three branches on Molex 39014041**, one per column.

12 V never leaves the converter's input side except to the underglow SM 3P and
the external barrel input.

This does **not** match what the pre-teardown design notes assume. `CLAUDE.md`
describes 12 V running "from the PSU terminals **directly** to each column's
first panel (**fork/spade lugs at the PSU end**)", and the master GND tie as "a
short lead to a **fork terminal on the PSU's GND stud**". The stock pad has no
such arrangement: the PSU is a brick with one YL 2-way output, and the columns
are on 5 V behind a converter.

These were **unconfirmed placeholders inherited from the pre-teardown design
notes, not observations** — and all of them are now **RESOLVED, 2026-08-16**:

- ✅ `power-column.yml` — `PSU_12V`/`PSU_GND` fork lugs replaced by
  `WAGO_12V`/`WAGO_GND` lever-block ports. Its 12 V premise **stands**: our
  panels are WS2815 and 12 V-native, so the converter is what goes away.
- ✅ `underglow.yml` — rewritten. 12 V and GND come from the fan-out; the master
  GND tie gets **its own lever port and its own lead** rather than a "PSU GND
  stud" or a ride on the underglow cable.
- ✅ The Underglow length row "SM 3P to PSU lugs" — gone with the rewrite.
- ✅ "PSU terminal screw size, for fork lug selection" — moot, as noted; the PSU
  has no screw terminals and nothing now needs a lug.

**What replaced them:** `12v-trunk.yml`. The stock 12 V star point (the
converter's input screw terminals) becomes a **Wago 221-415 lever-block fan-out**
mounted on the Daygreen's own two M3 holes (57 mm centres), fed from the PSU's
captive cable through an XT30 and an 8 A slow-blow fuse.

No PCB changes were implied by any of this — it is all upstream of the boards,
exactly as predicted when the question was first raised.

### 2. Connector gender — AUDITED 2026-08-08, one item still open

Every JST wire-to-wire connector recorded so far was re-checked against the
part in hand. **The whole YL side was labelled backwards**; the SM and VL parts
were right first time. The cause was JST's counterintuitive naming — receptacle
housings hold *male pin* contacts — and the fact that Molex uses the word the
opposite way. Full rule and the bench test in `PARTS.md`, "Connector gender
conventions". Corrections are applied to every `.yml`.

**This was a sourcing error, not a wiring error:** JST housings are mirror
images so pin 1 always mates pin 1, and the pinouts stayed self-consistent
throughout.

- [x] **Molex 39014041 gender — CLOSED.** The wire-side housing has **holes**,
      so it is a Molex *receptacle* with female 5556-family sockets and the part
      number was right. The "plug" reading came from the same
      it-plugs-in-so-it's-the-plug reasoning that inverted the whole JST YL side
      — in the opposite direction, because Molex names by contact and JST names
      by shell.
- [x] **`YL2` / `YLR_BARREL` are 2-position** — confirmed. Their pin numbering
      is still *derived* rather than read off the housings, and receptacle/plug
      mirror, so 1 and 2 could swap. Electrically moot: two circuits, and
      red/black and yellow/black are unambiguous. **Wire by color.**
      Rolled into the **pinout re-verification pass** above, which records
      physical position against the latch and so retires derivation entirely.

**All three gender predictions were later found and all three were correct** —
`YLP-01V`, `SMP-02V-BC` and `YLP-09V`.

Deliberately deferred: the unsourced interface connectors `J210–J213` /
`J301–J304`. Their gender call is now unambiguous (Molex receptacle = female on
the cable, mating a male-pin header on the board), but the sourcing decision
waits until the stock record is finished and our own harness work starts.

### 2. Anything else

Add anything here that does not fit a table above.
