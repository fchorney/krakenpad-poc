# Master Schematic Plan — pin map & blocks (v0.1, 2026-07-18; wiring current as of 2026-07-20)

Working plan for `hardware/master-pcb/`. Same process as the panel: this doc
first, then the KiCad project. Drawing workflow (agreed 2026-07-18): Claude
creates the project and places/annotates symbols; the user does all wiring from
a connection list.

The master is deliberately simple: **no 12V anywhere on the board, no
regulators, no magnetics.** Everything runs from the Teensy's USB power (5V
USB rail + Teensy's onboard 3.3V regulator). The 12V distribution goes
PSU → panel columns directly and never touches this PCB.

## Decisions locked 2026-07-18

| Item | Decision |
|------|----------|
| MCU | Teensy 4.0, **socketed** (female headers, replaceable) |
| INT connector | **9-position 5.08mm pluggable terminal block (Euroblock)** — no GND position needed (return rides the power ground network); 5.08 pitch for finger room. MPN at BOM time |
| INT pull-ups | 10kΩ to 3.3V (deliberately not stiff — INT-into-dead-panel case from the panel design review), **resistor array**, exact array format = whatever is cheapest at BOM time |
| INT ESD | **3× SRV05-4** (SOT-23-6 rail-clamp array, 4ch each; TECH PUBLIC, LCSC C558418, ~$0.013/pc — decided 2026-07-18). D2=INT0–3, D3=INT4–7, D4=INT8 (3 spare ch NC); VN→GND, VP→+3.3VDC. Prototype's single SMBJ3.3A retired |
| GND tie + underglow out | **Merged into ONE 2-position KF301-style 5.08mm screw terminal (decided 2026-07-18):** pos 1 = GND (the mandatory tie lead to the PSU GND stud), pos 2 = LED-DATA-OUT. Rationale: a separate GND pin on the underglow connector is redundant as a DC return (strips ground at the PSU lugs, common with the tie), but keeping a GND position *adjacent* to DATA preserves the option of a paired/twisted return wire alongside the 800kHz data line (small return loop, less ringing) — a 5.08mm position clamps two conductors fine. Whether the harness actually runs data+GND paired is decided at teardown. Data gating is UI/software config only ("underglow: on/off") — no sense pin for now; a 12V-sense position (panel R18/R19-style divider) is the known upgrade path if config-only gating proves annoying. **DRAWN 2026-07-20: J3 deleted, the surviving 2-pos part is J4 — pin 1 = DATA (from R3), pin 2 = GND.** Note the pin order is the reverse of this row's original "pos 1 = GND" wording; as-drawn wins, and nothing depends on which position is which |
| Player ID | 3-bit DIP switch to GND, Teensy internal pull-ups (no external resistors) |
| Status LED | 1× LED + series resistor on a spare GPIO (because why not) |
| RS-485 out | Micro-Fit 3.0 3-pin, THT right-angle 43650-0300 — same part and pin-3-unpopulated keying as the panels |
| RS-485 transceiver | THVD1429 (same as panel), 3.3V, DE+/RE tied to one GPIO |
| Termination | 120Ω fixed at the master end (master is always a bus end — no DPDT needed here, unlike panels) |
| RS-485 bias | **DNP pair added 2026-07-20** (panel review item 3.a): R4 390R 1% +3.3VDC→RS485+, R5 390R 1% RS485−→GND. Not fitted — the THVD1429's integrated open/short/idle failsafe covers the idle-bus case. Footprints exist so bias can be added at one bus point (the master, correctly) if the bench ever disagrees |

## Teensy 4.0 pin map (SUPERSEDED — original 2026-07-18 proposal, kept for rationale only)

> **STALE. Do not wire from this table.** The board is wired and verified; the
> authoritative map is **"As-built Teensy pin map"** near the end of this doc,
> generated from the netlist. What actually changed: pin *numbering* moved to the
> real PJRC scheme (the second edge row is 20–33, not 15–28), and the INT lines
> moved from GPIO 3–11 to **GPIO 15–23** so nothing sits on GPIO 13 (onboard LED).
> Only the hard constraints below still hold verbatim: UART1 on pins 0/1, and
> VIN as the 5V shifter source.

| Pin | Function | Notes |
|-----|----------|-------|
| 0 (RX1) | UART1 RX ← THVD1429 RO | **Hard constraint: UART1 = pins 0/1** |
| 1 (TX1) | UART1 TX → THVD1429 DI | **Hard constraint** |
| 2 | RS-485 DE + /RE (tied) | Matches prototype firmware |
| 3–11 | INT0–INT8 (panels 0–8) | Inputs, external 10k pull-up to 3.3V; any digital pin is interrupt-capable on Teensy 4.x, so this range is pure convenience |
| 12 | Underglow LED data out → shifter A input | 3.3V push-pull |
| 14, 15, 16 | Player-ID DIP bits 0–2 | To GND, `INPUT_PULLUP`, read at boot |
| 17 | Status LED | As wired 2026-07-18 (plan originally said 20 — any spare pin is fine; 13 avoided so the onboard LED stays independently usable) |
| 3.3V | THVD1429 VCC, INT pull-up array top | Teensy onboard regulator, 250mA budget — total load here is single-digit mA |
| VIN | SN74AHCT125 VCC (5V) | On Teensy 4.0 VIN is fed from USB 5V while the VUSB↔VIN pad link is intact (default). **Verify against the PJRC pinout card when drawing** |
| GND | Star point | Teensy GND + shifter GND + TVS returns + GND-tie terminal + underglow GND all meet here |

Spare after this: ~14 digital pins. Unused pins → nothing (no test-pad
obligation; add if board space is free, panel-style).

## Blocks

1. **Teensy 4.0 module (socketed)**: 2 female header strips on the PCB, Teensy
   with male pins plugs in. Symbol/footprint: source a Teensy 4.0
   symbol+footprint (PJRC publishes official KiCad libraries — verify pin
   count/geometry against the real module before footprint freeze). Only the
   outer edge pins are needed — no underside SMD pads used (USB host, VBAT
   etc. unused).
2. **RS-485**: THVD1429 SOIC-8. VCC = 3.3V + 100nF decoupling. RO→pin 0,
   DI→pin 1, DE+/RE tied→pin 2. A/B → Micro-Fit 43650-0300 (A=pin 1, B=pin 2,
   pin 3 unpopulated keying — match the panel J8/J10 pinout exactly so the
   inter-board cable is straight-through). 120Ω termination across A/B,
   always fitted.
3. **INT block**: 9-pos 5.08mm pluggable Euroblock. Per line: 10k pull-up to
   3.3V (array), TVS channel to GND (array), then to Teensy pins 3–11.
   Slot→panel mapping fixed by wire color (stock SMX map, see BOM.md):
   0=Red 1=Orange 2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black.
   Layout note for later: TVS + pull-ups + star ground live next to this
   connector.
4. **Underglow**: SN74AHCT125 (SOIC-14, same MPN as panel U4), VCC = 5V from
   VIN + 100nF. One gate used: A ← pin 12, /OE → GND, Y → ~330Ω series
   (mirrors panel R16) → DATA position of the merged GND-tie/underglow
   terminal (see block 5 and the decisions table). Three unused
   gates: tie inputs and /OE pins to GND (this board's shifter is always
   powered, unlike a future unpopulated-VCC scenario; the panel left its
   spares NC — either is defensible, GND-tied is the cleaner default for a
   fresh schematic).
5. **GND tie + underglow terminal (merged 2026-07-18, drawn 2026-07-20 as J4)**:
   one 2-pos KF301-style screw terminal — pin 1 = underglow DATA, pin 2 = GND
   lead to the PSU GND stud. The GND tie is not optional wiring for the user — INT and RS-485
   both need the common reference (documented bench failure mode without it).
   The underglow DATA position may simply sit empty if underglow is unused.
6. **Player-ID DIP**: 3-position DIP to GND on pins 14–16, internal pull-ups.
   Values 0–3 = P1–P4, 4–7 reserved.
7. **Status LED**: LED + 1kΩ on pin 17 (as wired; plan originally said 20),
   R2 on the anode side.
8. **RS-485 bias (R4/R5, DNP)**: 390R 1% each, +3.3VDC→RS485+ and
   RS485−→GND. Unpopulated by default; see the decisions table.
9. **Test points** (space permitting): RS-485 A, B, DE, 3.3V, 5V, GND,
   underglow data (both 3.3V and 5V sides).

## Deliberately absent (do not re-add)

- Any 12V input, distribution, or sensing — the master never sees 12V.
- Underglow 5V-in from the stock Daygreen converter — considered 2026-07-18,
  rejected: the Teensy USB rail already provides shifter VCC. The Daygreen
  brick remains unused by this design.
- Underglow presence sense — shelved with the 5V-in; UI gating for now,
  teardown may reopen it.
- GND position on the INT Euroblock — return is via the power ground network.
- RS-485 termination switch — master is always a bus end; fixed 120Ω.

## Open items (non-blocking, resolve by BOM/layout time)

- 10k resistor-array MPN/format (cheapest).
- Euroblock 9-pos 5.08mm MPN.
- 3-bit DIP switch MPN.
- Teensy 4.0 KiCad symbol/footprint source + verification.
- Underglow connector final form — 2-pos screw terminal is the interim
  decision; harness teardown decides the splice point and may change it.
- Master PCB target ~80×60mm; enclosure sized after layout.

## Wiring list (v0.1 schematic, 2026-07-18)

**STATUS: wired, cleaned up, and netlist-verified — current as of 2026-07-20.**
Every net below was checked pin-by-pin against a `kicad-cli` netlist export.
ERC is down to 4 `lib_symbol_mismatch` warnings on the 74AHCT125 units (the
known cached-vs-derived noise class) and **0 errors**; the floating no-connect
flags are gone.

Deviations from the list as originally written, all deliberate and all fine:

- **J3+J4 merge is now DRAWN** — J3 deleted, J4 is the surviving 2-pos screw
  terminal with pin 1 = DATA (via R3) and pin 2 = GND.
- **Status LED is on GPIO `17`**, not `20`, and R2 now sits on the **anode**
  side (GPIO17 → R2 → D1 anode, D1 cathode → GND). Moved there 2026-07-20;
  electrically identical to the cathode-side placement, clearer to read.
- **Rail names standardized** to the panel's ×VDC convention: `+3.3VDC` and
  `+5VDC_USB` (the `_USB` suffix is meaningful — this rail is the Teensy's
  USB 5V, a different source from the panel's +5VDC).
- **Spare Teensy pins carry readable net names** (`GPIO 13/LED`, `GPIO 18`
  through `GPIO 23`) instead of auto-generated `unconnected-(U1-…)` strings.
- **R4/R5 RS-485 bias pair added and wired** (review item 3.a): R4 390R 1%
  from +3.3VDC to RS485+, R5 390R 1% from RS485− to GND, **both DNP**. Fitted
  only if the THVD1429's integrated failsafe ever proves insufficient; the
  390R pair gives ≈236mV across the 60Ω loaded bus at 3.3V.

Parts are placed in `hardware/master-pcb/master-pcb.kicad_sch` (rev 0.1). Pin references below use the Teensy symbol's pin *names*
(GPIO numbers); the symbol itself is `master-pcb:Teensy_4.0` (DIP-28-style
numbering, custom — verify against the PJRC pinout card before layout).

**Power spine**
- U1 `3V3` → +3.3VDC rail (power symbol). U1 `VIN(USB5V)` → +5V rail — name it
  `+5VDC_USB` (it is the Teensy's USB 5V via the intact VUSB↔VIN link).
- U1 both `GND` pins, J4.2 (GND tie), all GND points below → GND.
- PWR_FLAG on +3.3VDC, +5VDC_USB, and GND (all are sourced by the module, not a
  KiCad power symbol, so ERC needs the flags).

**RS-485 (U2 THVD1429, C1, R1, J1)**
- U2.1 RO → U1 `0/RX1` · U2.4 DI → U1 `1/TX1` · U2.2 /RE + U2.3 DE tied
  together → U1 `2`
- U2.8 VCC → +3.3VDC, C1 across VCC/GND at U2 · U2.5 GND → GND
- U2.6 A → J1.1 and R1.1 · U2.7 B → J1.2 and R1.2 (R1 120Ω = always-fitted
  master-end termination) · J1.3 — leave unconnected (keying, matches panels)

**INT block (J2, RN1, D2–D4)**
- J2 pin n+1 = INTn from panel n (colors R,O,Y,G,Bu,Br,Gy,W,Bk)
- Each INTn: J2 pin → RN1 element pin (n+2) → Teensy GPIO(3+n) — i.e.
  INT0→`3` … INT8→`11` — and an SRV05-4 IO channel:
  D2 IO1–4 = INT0–3, D3 IO1–4 = INT4–7, D4 IO1 = INT8 (D4 IO2–4 leave NC)
- All three arrays: pin 2 VN → GND, pin 5 VP → +3.3VDC
- RN1 pin 1 (common) → +3.3VDC

**Underglow + GND tie (U3 gate A, R3, J4, C2)**
- U3A: pin 1 /1OE → GND · pin 2 1A → U1 `12` · pin 3 1Y → R3.1
- R3.2 → J4.1 (DATA) · J4.2 → GND (the merged GND-tie position)
- U3 pin 14 VCC → +5VDC_USB with C2 at the pin · pin 7 GND → GND
- Unused gates B/C/D: inputs (5, 9, 12) and /OEs (4, 10, 13) → GND,
  outputs (6, 8, 11) unconnected

**Player DIP (SW1) — GPIO `14`,`15`,`16`**, one switch each, other side of
all three → GND (internal pull-ups, no resistors).

**Status LED** — U1 `17` → R2 (1k) → D1 anode, D1 cathode → GND.

After wiring: annotate is already done (refs assigned); run ERC — expect it
to drop from ~128 unconnected-pin errors to ~spare-GPIO warnings only; the 7
`lib_symbol_mismatch` warnings are the known cached-vs-derived noise class
(same as the panel), verify-once then exclude.

---

## Teensy 4.0 symbol/footprint swap — 2026-07-21

**The hand-built `Teensy_4.0` symbol was retired and replaced with the upstream
[XenGi](https://github.com/XenGi/teensy_library) library (MIT).** Reason: the
hand-built symbol's pin numbering was **wrong on one whole row**.

It numbered the two long edges 1–14 and 15–28. The real Teensy 4.0 numbers them
**1–14 and 20–33**, because pads **15–19** are the five-pad row on the short
end (`VBAT`, `3V3`, `GND`, `PROGRAM`, `ON_OFF`) — which the hand-built symbol
omitted entirely. Every connection on the second row would have landed **5 pads
off** on the PCB, and neither ERC nor DRC would ever have flagged it.

Verified against the real board: pads 1–14 at y=+7.62, 20–33 at y=−7.62 (both
2.54 pitch, x −16.51→+16.51), 15–19 down the short end at x=+16.51, 34 = VUSB.
Pin span 33.02 × 15.24 mm = the correct 35.6 × 17.8 mm Teensy 4.0.

### What was vendored

| Asset | Path | Source |
|-------|------|--------|
| Symbol `Teensy4.0` (34 pins) | `master-pcb.kicad_sym` | XenGi/teensy_library, MIT, **locally trimmed** |
| Footprint `Teensy40_Socketed` (34 pads) | `master-pcb.pretty/` | XenGi/teensy.pretty, MIT, **locally modified** |
| 3D model | `3dmodels/Teensy_4.0_Assembly.STEP` | XenGi/teensy.pretty, MIT |

Footprint modification: upstream pads **35–44** (Teensy bottom-side pins 24–33)
were deleted. Unused on this design, and as through-holes they sat in a 2×5 grid
directly under the socket eating routing area. Pads 1–34 are untouched.
Attribution + licence in `hardware/master-pcb/3dmodels/README.md`.

Symbol modification: upstream pins **35–54** were deleted, leaving 1–34. See
"Why the symbol was trimmed" below — this is required to reach a clean parity
check, and is not optional cosmetics.

`fp-lib-table` was created (the project had none) pointing at `master-pcb.pretty`.

### Why the symbol was trimmed — parity

Upstream's symbol describes the **whole Teensy chip**: all 54 electrical
connection points, including the bottom-side SMT pads (pins 24–39, D+/D−, extra
GND/3V3). The footprint describes **what can actually be soldered on this
board**. Those are different sets, and KiCad cares.

**KiCad's schematic-parity check flags every symbol pin that has no matching
pad — even when the pin is unconnected.** Verified empirically on 2026-07-21 by
building a throwaway board and running
`kicad-cli pcb drc --schematic-parity --severity-all`; it emitted 20 violations
of the form `No pad found for pin NN in schematic`, one per padless pin, under
the `net_conflict` rule key.

Ten of those came from our pad strip. **The other ten (pins 45–54) are padless
in upstream's own symbol+footprint pairing** — XenGi ships a combination that
cannot pass parity. Trimming the symbol to 1–34 takes it to **0**.

Nothing wireable was lost. On a socketed Teensy the reachable pins are the two
14-pin long edges (1–14, 20–33) plus the 5-pad short-end row (15–19). Pins
35–54 are bottom-side SMT pads, physically unreachable through a socket.

### Short-end row (15–19) dropped — 2026-07-22

**Pins/pads 15–19 have since been removed as well**, from the symbol, the
`Teensy40_Socketed` footprint, and the schematic instance. The five holes sat
mid-board and were blocking routing; the row is now unpopulated (the socket is
the two 14-pin edge strips only, which is how a Teensy is normally socketed
anyway). Symbol, footprint, and schematic instance all now carry **29** pins
(1–14, 20–34), so parity stays at 0.

What was on them, and why each was free to lose:

| pin | signal | disposition |
|----:|--------|-------------|
| 15 | `VBAT` | unused — RTC coin-cell backup is not a feature of this design |
| 16 | `VDD` | **redundant.** Was a `hide`den pin stacked at (3.81, 43.18) directly on visible pin 31, same `+3.3VDC` net, same internal rail on the Teensy |
| 17 | `GND` | **redundant.** Hidden pin stacked at (0, 0) on pin 1, with pin 32 stacked there too — `GND` keeps pins 1 and 32 |
| 18 | `PROGRAM` | external program/reset button. Dropped deliberately: routing it out only buys an *enclosure-exterior* button vs. opening the lid to reach the Teensy's own button — and flashing requires plugging in USB at the box regardless, so the delta is one screwdriver on a path that Teensyduino's USB auto-reboot makes near-unreachable |
| 19 | `ON_OFF` | unused — external power-down control |

Load check: the `+3.3VDC` rail here is the THVD1429 plus the 10k pull-up array,
single-digit mA. One supply pin and two grounds is ample.

Because 16/17 were *stacked hidden* pins rather than separately wired ones,
**no schematic wires had to be deleted** — they took their nets by coincident
placement. On the PCB, pads 15–19 carried **no net at all** and had nothing
routed to them, so board connectivity is unchanged by the removal.

**Pad 34 (`VUSB`) is retained but is also a bottom pad** — the footprint places
it at (−13.97, −5.08), i.e. board interior, not the ±7.62 edge rows. It stays as
a through-hole under the socket, usable only if a flying lead is soldered to the
Teensy before seating it.

### As-built Teensy pin map (verified from netlist 2026-07-22)

Re-wiring after the symbol swap is **done**. Pin numbers below are the symbol/
footprint pad numbers; `GPIO_n` is the Teensy signal name. Verified directly
against `kicad-cli sch export netlist` — this table is generated, not hand-kept.

| U1 pin | symbol pin | net | connects to |
|-------:|------------|-----|-------------|
| 1 | `GND` | `GND` | _23 nodes_ |
| 2 | `GPIO_0` | `RS485_RX` | `U2.1` |
| 3 | `GPIO_1` | `RS485_TX` | `U2.4` |
| 4 | `GPIO_2` | _spare_ | — |
| 5 | `GPIO_3` | `RS485_DE` | `U2.2`, `U2.3` |
| 6 | `GPIO_4` | _spare_ | — |
| 7 | `GPIO_5` | `DIP_ID0` | `SW1.1` |
| 8 | `GPIO_6` | `DIP_ID1` | `SW1.2` |
| 9 | `GPIO_7` | `DIP_ID2` | `SW1.3` |
| 10 | `GPIO_8` | _spare_ | — |
| 11 | `GPIO_9` | `STATUS_LED` | `R2.2` |
| 12 | `GPIO_10` | _spare_ | — |
| 13 | `GPIO_11` | `UNDERGLOW_DATA` | `U3.2` |
| 14 | `GPIO_12` | _spare_ | — |
| ~~15–19~~ | — | — | **removed 2026-07-22**, see above |
| 20 | `GPIO_13` | _spare_ | — |
| 21 | `GPIO_14` | _spare_ | — |
| 22 | `GPIO_15` | `INT_UL` | `D2.1`, `J2.1`, `RN1.2` |
| 23 | `GPIO_16` | `INT_U` | `D2.3`, `J2.2`, `RN1.3` |
| 24 | `GPIO_17` | `INT_UR` | `D2.4`, `J2.3`, `RN1.4` |
| 25 | `GPIO_18` | `INT_L` | `D2.6`, `J2.4`, `RN1.5` |
| 26 | `GPIO_19` | `INT_C` | `D3.1`, `J2.5`, `RN1.6` |
| 27 | `GPIO_20` | `INT_R` | `D3.3`, `J2.6`, `RN1.7` |
| 28 | `GPIO_21` | `INT_DL` | `D3.4`, `J2.7`, `RN1.8` |
| 29 | `GPIO_22` | `INT_D` | `D3.6`, `J2.8`, `RN1.9` |
| 30 | `GPIO_23` | `INT_DR` | `D4.1`, `J2.9`, `RN1.10` |
| 31 | `VDD` | `+3.3VDC` | _9 nodes_ |
| 32 | `GND` | `GND` | _23 nodes_ |
| 33 | `VIN` | `+5VDC_USB` | `C2.1`, `U3.14` |
| 34 | `VUSB` | _spare_ | — |

**RS-485 remains on GPIO 0/1 = Serial1**, the one hard pin constraint on this
board (TX/RX must sit on a matched hardware UART pair). Reachable pairs on a
socketed Teensy 4.0: Serial1 `0/1`, Serial2 `7/8`, Serial3 `15/14`,
Serial4 `16/17`, Serial5 `21/20`. Serial6/7 are bottom-side pads — not options.
Everything else (INT lines, DIP, status LED, underglow data) is unconstrained;
all Teensy 4.x digital pins are interrupt-capable.

**GPIO 13 (pin 20) is deliberately left spare** — it drives the Teensy's onboard
LED, so an INT input there would fight the LED circuit against the 10k pull-up.
The nine INT lines were moved off it onto GPIO 15–23.

#### Bug caught during verification — INT_DL / INT_DR short (FIXED)

The netlist review found `INT_DL` carrying **8 nodes** while every other INT net
had 4, and no `INT_DR` net existing at all: the J2.9 / D4.1 / RN1.10 line was
labelled `INT_DL`, and identical global labels merge. Panels 6 (DL) and 8 (DR)
were electrically common — a DL press would have fired both GPIOs and the master
could never have told them apart. **ERC cannot catch this**; both connections are
individually legal. Fixed by relabelling to `INT_DR`.

Lesson worth keeping: after any bulk re-wire, diff the netlist and check that
each repeated-structure net has the *same node count*. An outlier is the tell.

#### Library symbol mismatches — resolved, 0 exclusions

ERC's 8 `lib_symbol_mismatch` warnings were fixed at the cause, in **two
opposite directions** — getting this backwards destroys work:

| Symbol | Cause | Direction |
|--------|-------|-----------|
| `Teensy4.0` | schematic cache held the **reflowed/renamed symbol**; `master-pcb.kicad_sym` still had the original vendored copy | **cache → library** |
| `74AHCT125` (×5) | stale Datasheet / Description / `ki_fp_filters` / `ki_keywords` | library → cache |
| `THVD1420D` | same, plus cache had been customised to carry THVD1429 identity | library → cache |
| `Conn_01x02` | cache was **missing two `(rectangle)` body graphics** | library → cache (whole block) |

The project convention — confirmed against panel-pcb, where cache and library are
byte-identical for both stock and project symbols — is **cache = pristine library
copy; project-specific identity lives on the instance**. `U2` already carried
Value `THVD1429`, the 1429 datasheet, LCSC and MPN on the instance, so resyncing
its cache to stock `THVD1420D` lost nothing.

**Do not run "Update Symbols from Library" blindly on this project** — it pushes
library → cache for everything, which would have silently reverted the Teensy
reflow. Check which side holds the newer edit first.

---

## Assembly strategy — hand-assembled, bare PCB only (decided 2026-07-21)

**The master is NOT sent for PCBA.** JLC (or anyone) fabricates bare 4-layer
boards; all 19 components are hand-soldered. Rationale: on the panel's qty-5
quote, ~$148 of $240.78 was **qty-independent PCBA overhead** (setup, stencil,
feeder/extended-part fees). The master has 19 parts assembled once, so that
overhead buys almost nothing. Expected saving ≈ **$150–200 at qty 5**.

Consequences, all good:
- No CPL file, no stencil, no DNP-flag handling for the assembler.
- **Sourcing is no longer constrained to LCSC/JLC stock.** MPN is what matters;
  the LCSC C-number is a convenience. This is what unblocked the 9-position
  Euroblock and the SIP-10 array, whose LCSC availability could not be confirmed.

### Two "go full THT" traps that were evaluated and rejected

Hand-soldering a handful of non-tiny SMD parts is fine, so neither substitution
below is worth making. **Do not revisit these without new information:**

- **U2 THVD1429 is SOIC-8 only — no DIP exists.** The THT candidate was
  MAX3485CPA (DIP-8, 3.3V, 10Mbps, LCSC `C9944`). Rejected: it loses the
  integrated open/short/idle failsafe *and* the surge protection that motivated
  the 1429 in the first place, and it breaks part commonality with the nine
  panels. SOIC-8 at 1.27mm pitch hand-solders easily.
- **D2–D4 SRV05-4 is SOT-23-6.** The THT candidate was Littelfuse SP721APP
  (DIP-8, 6 channels, LCSC `C5373324`) — 2 needed at **~$1.62 each ≈ $3.25**
  versus 3× SRV05-4 at **~$0.013 ≈ $0.04**. ~80× the cost, bulkier, for a part
  that is hand-solderable at 0.95mm pitch. Keep the SRV05-4s.

**Hot air is available** (confirmed 2026-07-22), so package size is not a
constraint on this board. SOT-23-6 (0.95mm pitch, the finest thing here — finer
than the SOIC-8) is straightforward with paste + hot air; no stencil is worth
ordering for 4 chips and 7 passives. Do not re-litigate the package choices
above on hand-solderability grounds.

**DECIDED 2026-07-22 — passives are 0805.** All eight SMD passives (D1, R1–R5,
C1, C2) were swapped from 0603 to 0805 before placement, so the larger land pads
cost nothing in layout. Rationale: the board is not dense, everything is
hand-placed, and 0805 is easier to tweeze and rework. The "panel leftovers"
argument that had favoured 0603 was withdrawn as incorrect (see the correction
below) — loose passives are ordered fresh either way.

> **LCSC part numbers were CLEARED on those eight parts, not carried over.** The
> old C-numbers (`C22787`, `C2907002`, `C23138`, `C14663`, `C47416536`) are
> **0603-specific**; leaving them attached to 0805 footprints would have been an
> order-time trap. Values are unchanged and package-independent. **Re-pick all
> eight in the live LCSC dialog at order time** — that is the only trustworthy
> source (auto-matchers have served dead listings before).
>
> Unverified starting candidates from a web search, to save time but **confirm
> each live**: 1k 0805 1% `C2889437` (VO), 120R 0805 `C269726` (TyoHM),
> 330R 0805 1% `C101661` (LIZ). No confident candidate was found for the
> 100nF 0805 X7R 50V, the 390R 1% 0805, or the 0805 LED.

**Layout consequences of hand assembly:**
- Leave hot-air approach room around D2–D4 and the 0805s. They belong next to
  J2 (correct electrically — TVS and pull-ups at the connector), but must not be
  tucked against the 9-position block's body or the Teensy socket, both tall.
- Do not place SMD parts under the Teensy socket footprint.
- **Check SRV05-4 pin 1 orientation at placement** — a rotated rail-clamp array
  looks fine and silently fails to protect.

One part genuinely *improved* going THT: **U3 → SN74AHCT125N (DIP-14, LCSC
`C354152`)** — which is the exact part CLAUDE.md already named for the
prototype, so this is a return to intent, not a change.

Passives are **0805** (decided 2026-07-22 — see the decision block above).
**D1 is SMD too**, changed from a 3mm THT LED the same day. Their LCSC part
numbers were cleared in the 0603→0805 swap and must be re-picked at order time;
values are unchanged.

> **CORRECTION 2026-07-22 — there are no "panel leftovers".** Earlier revisions
> of this section claimed panel spares would cover the master's passives. That
> was **wrong**: the panel is JLC **PCBA** (110 SMD placements, JLC sources and
> places them); only the panel's through-hole parts are hand-soldered. No loose
> SMD passives ever reach the bench. The real carryover is only that these
> **part picks are already researched and verified** — a time saving, not a cost
> or stock saving. Loose passives must be ordered for the master regardless of
> the size chosen.

### Footprints as assigned 2026-07-21 (schematic now fully populated)

| Ref | Value | Footprint | Mount | MPN / LCSC |
|-----|-------|-----------|-------|------------|
| U1 | Teensy 4.0 | `master-pcb:Teensy40_Socketed` | THT socket | — |
| U2 | THVD1429 | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` | **SMD hand** | THVD1429DR / C1850236 |
| U3 | SN74AHCT125 | `Package_DIP:DIP-14_W7.62mm` | THT | SN74AHCT125N / C354152 |
| D1 | STATUS | `LED_SMD:LED_0805_2012Metric` | SMD | **re-pick** |
| D2–D4 | SRV05-4 | `Package_TO_SOT_SMD:SOT-23-6` | **SMD hand** | SRV05-4 / C558418 |
| J1 | RS-485 OUT | `Connector_Molex:Molex_Micro-Fit_3.0_43650-0300_1x03_P3.00mm_Horizontal` | THT | 43650-0300 |
| J2 | INT IN 0–8 | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-9-5.08_1x09_P5.08mm_Horizontal` | THT | **stand-in — see below** |
| J4 | UNDERGLOW OUT | `master-pcb:TerminalBlock_KF301-2P_P5.08mm` | THT | KF301-5.0-2P / C474881 |
| R1/R2/R3 | 120R / 1k / 330R | `Resistor_SMD:R_0805_2012Metric` | SMD | **re-pick** |
| R4/R5 | 390R 1% **DNP** | `Resistor_SMD:R_0805_2012Metric` | SMD | **re-pick** |
| RN1 | 10k ×9 | `Resistor_THT:R_Array_SIP10` | THT | A10-103JP |
| C1/C2 | 100nF X7R 50V | `Capacitor_SMD:C_0805_2012Metric` | SMD | **re-pick** |
| SW1 | PLAYER_ID | `Button_Switch_THT:SW_DIP_SPSTx03_Slide_9.78x9.8mm_W7.62mm_P2.54mm` | THT | DSWB03LHGET / C99420 |

**J2 footprint is a stand-in.** The plan calls for a *pluggable* Euroblock, but
KiCad ships no 5.08mm 9-position pluggable-header footprint. The Phoenix MKDS
1x09 P5.08mm Horizontal has the correct pitch and drill (2.6mm pad / 1.3mm
drill — **identical to the panel's KF301 footprint**), so layout is valid; only
the body outline/silk differs. Same stand-in approach the panel used for J9.
**Verify drill, pad and body envelope against the real part before ordering.**
Note the plan pre-approves **9 *or* 10 position** ("+ spare, fine if empty"),
which widens sourcing considerably if 9P proves awkward.

### Parts resolved 2026-07-21

- **SW1 = DSWB03LHGET (LCSC `C99420`)** — exact 3-position sibling of the panel's
  4-position DSWB04LHGET (`C99418`), same YE/KINGTEK family.
- **RN1 = A10-103JP** (FH, SIP-10, 10k, 9 bussed elements). Naming confirmed:
  `A09` = SIP-9 → 8 resistors, `A10` = SIP-10 → **9** resistors, which is what
  the `R_Network09` symbol needs. LCSC stocks A08/A09; A10 C-number unconfirmed,
  but sourcing is unconstrained now. Discrete 9× 10k 0805 remains a valid
  fallback — for *hand* assembly the array wins (10 easy THT joints vs 18 SMD).
- **J2** — family identified (Cixi Kefa KF2EDGR-5.08-9P / DORABO DB2ERC-5.08-9P),
  specific 9P C-number unconfirmed. Buy from any distributor.

### Test points — PLACED, PARKED, AWAITING WIRING

Eight `Connector:TestPoint` symbols (`TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm`,
the panel's standard probe-hole pad) sit unwired in a row at y=50.8,
x=38.1→180.34. ERC currently reports exactly 8 `pin_not_connected` — that is
these, and it clears when they are wired. **Wire each TP pin 1 to:**

| Ref | Connect to |
|-----|-----------|
| TP1 | `/RS485+` (J1.1 / R1.1 / U2.6) |
| TP2 | `/RS485-` (J1.2 / R1.2 / U2.7) |
| TP3 | `RS485_DE` (U1 pin 5 / U2.2 / U2.3) |
| TP4 | `+3.3VDC` |
| TP5 | `+5VDC_USB` |
| TP6 | `GND` |
| TP7 | `UNDERGLOW_DATA` — 3.3V side, U1 pin 13 → U3.2 |
| TP8 | underglow 5V side — U3.3 output, after R3 (the shifted signal) |

### RS-485 bias note

A schematic text note was added by the RS-485 block. It is deliberately **the
opposite** of the panel's note: the panel says "no external bus biasing", which
would contradict this board, which *does* carry R4/R5 bias footprints. The
master note explains they are DNP insurance, that the THVD1429's integrated
failsafe already defines the idle bus, and that bias belongs at exactly one
point on the bus — the master — so if it is ever needed it gets populated here
and nowhere else.

---

## INT ESD rework: SRV05-4 arrays → 9 discrete TVS — 2026-07-22

**Decision:** replace the 3× SRV05-4 rail-clamp arrays (D2/D3/D4) with **nine
single-line unidirectional TVS diodes**, one per INT line, placed at the J2
Euroblock. This supersedes the 2026-07-18 SRV05-4 decision recorded in the
decisions table above.

### Why

- **Routing.** A single-line TVS is a shunt stub: J2 pin → TVS pad → other pad
  → via straight down to the GND plane. Nine of those in a row parallel to the
  connector, no crossings. The arrays force four INT lines to converge on one
  6-pin part and fan back out to the right pull-up element and the right Teensy
  pin — that convergence was the whole tangle.
- **Clamp quality.** Shorter loop from the protected line to plane means lower
  clamp voltage than an array four lines are funnelling into.
- **Placement freedom.** J2 pin order and Teensy pin order no longer have to
  agree, because nothing groups INT lines in fours any more.
- **Closes the open SRV05 decoupling question** — there is no longer a VP rail
  pin to decouple. The `+3.3VDC` connection at the arrays disappears entirely
  (RN1.1 still needs it, so `+3.3VDC` still reaches the INT block).
- Cost delta is noise: ~$0.15 for nine vs ~$0.04 for three.

The "SOT-23-6 is hard to hand-solder" worry was **not** a driver — 0.95mm pitch
is easier than the 0402s already on the panel board. Routing was the real
argument.

### Part spec (pick at LCSC, prefer a JLC basic part)

| Parameter | Target |
|-----------|--------|
| Type | Unidirectional TVS (ESD suppressor) |
| Standoff `VRWM` | ≥ 3.6V, ≤ 5V |
| Breakdown `VBR` | ~6V |
| Clamping `VCL` | ≤ 10V |
| ESD rating | IEC 61000-4-2 ±8kV contact or better |
| Capacitance | **don't care** — INT lines are DC-slow; low-cap parts buy nothing here |
| Package | 0805 (`Diode_SMD:D_0805_2012Metric`); SOD-323 acceptable if the basic-part selection is better there |

Unidirectional over bidirectional: the line only ever sits between 0V and 3.3V,
so negative transients simply forward-conduct into GND at ~0.7V, which is a
better clamp than a bidirectional part's −5V. The tradeoff is that orientation
now matters at assembly.

### Symbol / footprint

- Symbol: **`Device:D_Zener`** — pin **1 = K** (cathode), pin **2 = A** (anode).
  Do *not* use `Device:D_TVS`; that symbol is the bidirectional back-to-back
  graphic with pins named A1/A2, which would misrepresent a unidirectional part.
- Footprint: **`Diode_SMD:D_0805_2012Metric`** (pad 1 = cathode, KiCad convention).
- Value: placeholder `TVS 5V` until the LCSC pick is made, then set Value to the
  MPN and add an `LCSC` property field to match the panel board's convention.

### Connection list — USER DRAWS

**Delete first:** D2, D3, D4 (all three SRV05-4), along with their wires, their
`GND` connections (pin 2 / VN) and their `+3.3VDC` connections (pin 5 / VP).

**Then place nine `Device:D_Zener`, D2 … D10.** Each one is identical:
**pin 1 (K) → the INT net; pin 2 (A) → GND.** Cathode to the signal, anode to
ground.

| Ref | Panel | Net | Also on this net |
|-----|-------|-----|------------------|
| D2  | 0 (UL) | `INT_UL` | J2.1, RN1.2, Teensy pin 22 (`GPIO_15`) |
| D3  | 1 (U)  | `INT_U`  | J2.2, RN1.3, Teensy pin 23 (`GPIO_16`) |
| D4  | 2 (UR) | `INT_UR` | J2.3, RN1.4, Teensy pin 24 (`GPIO_17`) |
| D5  | 3 (L)  | `INT_L`  | J2.4, RN1.5, Teensy pin 25 (`GPIO_18`) |
| D6  | 4 (C)  | `INT_C`  | J2.5, RN1.6, Teensy pin 26 (`GPIO_19`) |
| D7  | 5 (R)  | `INT_R`  | J2.6, RN1.7, Teensy pin 27 (`GPIO_20`) |
| D8  | 6 (DL) | `INT_DL` | J2.7, RN1.8, Teensy pin 28 (`GPIO_21`) |
| D9  | 7 (D)  | `INT_D`  | J2.8, RN1.9, Teensy pin 29 (`GPIO_22`) |
| D10 | 8 (DR) | `INT_DR` | J2.9, RN1.10, Teensy pin 30 (`GPIO_23`) |

Nothing else in the INT block changes: RN1 stays as-is (pin 1 common →
`+3.3VDC`, 10k per line, deliberately not stiff for the INT-into-dead-panel
case), J2 stays a 9-position 5.08mm Euroblock, and the Teensy pin assignments
are untouched.

### Layout note for when this reaches the PCB

Each TVS goes **between J2 and everything else** — first thing the line meets
coming off the connector pin, before it heads toward RN1 or the Teensy. Anode
pad gets its own GND via straight down to the plane, not a shared stub.

---

## Layer stackup — decided 2026-07-22

**4 layers: `Sig+Pwr / GND / GND / Sig+Pwr`.**

The useful framing is not "two ground planes" but **each outer signal layer gets
its own adjacent reference plane** — F.Cu references In1.Cu, B.Cu references
In2.Cu. Every trace on either side has a continuous return directly beneath it,
and every signal via between the outer layers lands next to a stitching via.
That removes the need to think carefully about which side a trace runs on, which
is the whole point on a board this simple.

No dedicated power plane, and that costs nothing here: the master carries logic
current only (USB-powered Teensy, one transceiver, one level shifter, the INT
pull-up array). There is **no 12V distribution on this board at all** — power
runs PSU → columns directly. Pours and fat traces on the outer layers are
plenty.

Contrast with the panel PCB, which needed real inner power pours for the 12V LED
rail; the master has no equivalent load.

### Applied 2026-07-22

Both `master-pcb.kicad_pcb` **and `panel-pcb.kicad_pcb`** carried KiCad's default
symmetric 0.48 / 0.48 / 0.48mm dielectric, which is not a real JLC stackup. Both
were set to JLC's published **JLC04161H-7628** 4-layer 1.6mm geometry:

| Layer | Type | Thickness | Er |
|-------|------|-----------|-----|
| F.Cu | copper 1oz | 0.035 | — |
| dielectric 1 | prepreg 7628 | 0.2104 | 4.4 |
| In1.Cu | copper 0.5oz | 0.0152 | — |
| dielectric 2 | core FR4 | 1.065 | 4.6 |
| In2.Cu | copper 0.5oz | 0.0152 | — |
| dielectric 3 | prepreg 7628 | 0.2104 | 4.4 |
| B.Cu | copper 1oz | 0.035 | — |

Sums to 1.5862mm ≈ 1.6mm. `loss_tangent` left at 0.02 (JLC does not publish it
on the impedance page; 0.02 is a reasonable FR4 figure).

**This does not change any fab output.** Gerbers are 2D and carry no stackup;
DRC does not read it. It affects the impedance calculator, the 3D view, and the
displayed board thickness only. The panel PCB was re-run through DRC after the
change and is still **0 violations / 0 unconnected** — its order-readiness is
unaffected and the production files do not need regenerating (the `.gbrjob`
file does embed stackup, so regenerate that if it matters to you).

The asymmetry works *in favour* of the master's plan — the outer layers sit
tight (0.21mm) against their reference planes, and the two GND planes being far
apart across the 1.065mm core is irrelevant since they are the same net.

### Master inner layers → both GND (applied 2026-07-22)

- Layer user-names: In1.Cu `GND_plane` → `GND`, In2.Cu `PWR_pours` → `GND`.
- **In2 zone converted from a `+3.3VDC` pour to `GND`** (renamed `GND_In2`) —
  renaming the layer alone would have left a 3.3V plane sitting on a layer
  labelled GND.

Consequence, which is the real work this creates: **the +3.3V net was being
distributed by that plane and now has to be routed on the outer layers.** DRC
delta after the change is confined entirely to `+3.3VDC` (unconnected items on
that net 8 → 14; every other net unchanged). Specifically these were living in
the old pour and are now floating or clashing:

| Item | Location |
|------|----------|
| U1 pad 31 `+3.3VDC` (Teensy 3V3 out) | (106.57, 106.38) |
| RN1 pad 1 `+3.3VDC` (pull-up array common) | (106.57, 101.00) |
| Via `+3.3VDC` | (138.50, 119.00) |
| Via `+3.3VDC` | (127.50, 128.00) |

U1.31 and RN1.1 are 5.38mm apart on the same X — they were connected *purely*
through the plane. Route them on F.Cu/B.Cu. There is no current problem doing
this: the whole 3.3V load is the THVD1429 plus the pull-up array, single-digit
mA, so an ordinary trace is more than sufficient. Also expect one
`isolated_copper` on the new GND_In2 fill until the area is re-poured/stitched.

**Note:** both inner layers now carry the *same* user-name `GND`. `kicad-cli`
loads the board fine, but confirm the Board Setup dialog accepts the duplicate
when you open it — if it insists on unique names, use `GND` / `GND2`.

**Still to do:**

- Stitch In1 and In2 together liberally — they are one net and should behave
  like one. Concentrate vias around the J2 INT connector, the THVD1429, and the
  Teensy.
