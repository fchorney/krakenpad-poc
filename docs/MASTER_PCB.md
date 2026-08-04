# Master PCB — as-built reference

Condensed 2026-07-23 from `MASTER_SCHEMATIC_PLAN.md` (drafting history,
including the retired SRV05-4 TVS approach and the original Serial1 pin plan,
lives in git). Everything below is verified against the current schematic
netlist.

**Status:** schematic and layout complete, **ERC/DRC clean (both re-run
2026-07-24)** — **not yet ordered.** External AI review (2026-07-23) triaged +
reworked 2026-07-24; **no fab-blockers** — the review's flagged SW1↔Teensy-socket
"short" was a false positive (a rotation-sign error in the manual pad-overlap
math; KiCad's clean copper-clearance DRC was correct all along).

The master is deliberately simple: **no 12V anywhere on the board, no
regulators, no magnetics.** Everything runs from the Teensy's USB power
(+5VDC_USB rail from VIN + the Teensy's onboard 3.3V regulator). 12V runs
PSU → panel columns directly and never touches this PCB. **The master's GND
must be tied to the PSU ground stud** (J2 pin 2) — INT and RS-485 need the
common reference; separate grounds was a real bench failure mode.

**Assembly: bare PCB fab only, hand-soldered** (PCBA's ~$148 fixed overhead
still isn't worth it at this volume × 2 boards; the 2026-07-24 INT-filter
additions — 9× 330R + 9× 1nF, all 0805 — roughly double the passive count but
stay easy to hand-place). Sourcing: the whole master is
hand-assembled from the LCSC order — see `docs/BOM.md` order 2. Hot air available; SOT-23-6 and
SOIC-8 are fine. Do not re-litigate: THVD1429 has no DIP equivalent worth the
downgrade (MAX3485CPA loses failsafe + surge), and the passives are 0805 by
choice (hand-placed, not dense).

## Teensy 4.0 pin map (as built, netlist-verified 2026-07-23)

Socketed on two 14-pin edge rows (pads 1–14 and 20–33 = 28 pads; the
short-end 15–19 row, the bottom-side SMT pads, and pad 34/VUSB were all
**deliberately removed** from the symbol/footprint — unreachable or unused
through a socket, and trimming is required for a clean schematic-parity
check). Symbol/footprint vendored from
XenGi/teensy_library (MIT), locally trimmed; 3D model + attribution in
`hardware/master-pcb/3dmodels/`.

| Pad | GPIO | Net | Connects to |
|----:|------|-----|-------------|
| 1, 32 | GND | `GND` | star point |
| 5 | 3 | `DIP_ID2` | SW1 |
| 6 | 4 | `DIP_ID1` | SW1 |
| 7 | 5 | `DIP_ID0` | SW1 |
| 8 | 6 | `RS485_DE` | U1 DE + /RE |
| 9 | 7 | `RS485_RX` | U1 RO — **Serial2 (RX2)** |
| 10 | 8 | `RS485_TX` | U1 DI — **Serial2 (TX2)** |
| 13 | 11 | `UNDERGLOW_DATA` | U3 A input + **R4 10k pull-down → GND** (holds U3 input LOW at boot, before firmware drives the pin) |
| 22–30 | 15–23 | `INT_DR` … `INT_UL` | see INT table below |
| 31 | — | `+3.3VDC` | Teensy 3V3 out → U1 VCC, RN1 common, TP6 |
| 33 | — | `+5VDC_USB` | Teensy VIN (USB 5V via intact VUSB↔VIN link) → U3 VCC. **The board taps VIN, not the raw VUSB pad — the on-Teensy VUSB↔VIN bridge must stay intact (do not cut it), or U3 loses its 5V supply.** |
| spare | 0, 1, 2, 9, 10, 12, 13, 14 | — | **GPIO 0/1 (Serial1) kept free on purpose** — the last spare hardware UART (see below). GPIO13 = Teensy onboard LED, now the status LED |

**No board status LED (removed 2026-07-24):** the discrete status LED and its series resistor were
dropped as redundant — the Teensy's onboard LED (GPIO13) serves as the status indicator, so
firmware just drives pin 13. GPIO1 (freed) went to the DIP.

### Pin-capability audit (2026-08-04)

Every assignment re-derived from the schematic netlist and checked against what
the pin can actually do. **No blockers.**

- ✅ **RS-485 is on a real hardware UART pair.** Teensy 4.0's **Serial2 is RX = 7,
  TX = 8**, which is exactly how it is wired. Not bit-banged, not a re-mapped pair.
- ✅ **`RS485_DE` on GPIO6** — Teensyduino's `Serial2.transmitterEnable(6)` accepts
  any digital pin and handles assert/release around the transmission, so unlike the
  panel side this does not need hand-timed DE. (The panel has no equivalent; see
  `docs/DUAL_PANEL.md`.)
- ✅ **All nine INT inputs are interrupt-capable** — every Teensy 4.0 digital pin
  supports `attachInterrupt`, so GPIO15–23 are unconstrained.
- ✅ **`UNDERGLOW_DATA` on GPIO11.** Bit-banging WS281x timing works on any pin, but
  note GPIO11 is also **MOSI** — driving the strip from hardware SPI is available if
  bit-banging ever proves jittery. R4's 10 k pull-down holds U3's input LOW at boot,
  before firmware drives the pin.
- ✅ **DIP on GPIO3/4/5** — plain inputs, `INPUT_PULLUP`, no board resistors.

**Worth confirming at bring-up, not verified here:** GPIO15–23 are all `AD_B1_xx`
pads, which sit on a single i.MX RT GPIO port. If that holds, **all nine INT lines
can be sampled in one register read** — useful both for the glitch-qualify re-read
and for the `'I'` identify self-test, which has to watch all nine at once. This is a
processor-reference claim, not something derived from the board files, so check it
with a one-liner rather than assuming it.

The one hard constraint is that RS-485 TX/RX sit on a matched hardware UART
pair — **as built that's Serial2 (GPIO 7/8)**. The INT lines occupy GPIO 15–23,
which blocks Serial3 (14/15), Serial4 (16/17), and Serial5 (20/21); with Serial2
used by RS-485, that leaves **Serial1 (0/1) as the only free hardware UART** on
the socketed pins — which is why the DIP was placed on GPIO 3/4/5 (plain GPIOs)
rather than 0/1, keeping Serial1 open for a future debug/aux UART.
All digital pins are interrupt-capable, so the INT lines are unconstrained.

### INT block mapping (connector ↔ panel ↔ GPIO)

**Superseded 2026-07-26: the 9-position Euroblock was replaced by nine
discrete JST XH 2-pin connectors**, one per panel, each carrying INT signal +
a dedicated GND return (see "INT cabling" below). The old Euroblock's "position 1 = panel
8" reversal note no longer applies — **connectors now run left-to-right across
the board in panel order 0→8**, silkscreened with the panel's position name
(`UL`, `U`, `UR`, `L`, `C`, `R`, `DL`, `D`, `DR`) rather than a refdes, so the
refdes numbering below never has to be read during assembly.

Every connector: **pin 1 = INT signal, pin 2 = GND** (matches the panel's J214).

| Silk | Conn | Net | Panel | Color | TVS | Series R | Filter C | RN1 | Teensy pad / GPIO |
|------|------|-----|-------|-------|-----|----------|----------|-----|-------------------|
| `UL` | J11 | `INT_UL` | 0 (UL) | Red | D1 | R6 | C3 | .2 | 30 / GPIO23 |
| `U` | J10 | `INT_U` | 1 (U) | Orange | D2 | R7 | C4 | .3 | 29 / GPIO22 |
| `UR` | J9 | `INT_UR` | 2 (UR) | Yellow | D3 | R8 | C5 | .4 | 28 / GPIO21 |
| `L` | J8 | `INT_L` | 3 (L) | Green | D4 | R9 | C6 | .5 | 27 / GPIO20 |
| `C` | J7 | `INT_C` | 4 (C) | Blue | D5 | R10 | C7 | .6 | 26 / GPIO19 |
| `R` | J6 | `INT_R` | 5 (R) | Brown | D6 | R11 | C8 | .7 | 25 / GPIO18 |
| `DL` | J5 | `INT_DL` | 6 (DL) | Grey | D7 | R12 | C9 | .8 | 24 / GPIO17 |
| `D` | J4 | `INT_D` | 7 (D) | White | D8 | R13 | C10 | .9 | 23 / GPIO16 |
| `DR` | J3 | `INT_DR` | 8 (DR) | Black | D9 | R14 | C11 | .10 | 22 / GPIO15 |

Refdes were made contiguous by the 2026-07-31 reannotation: J1 = RS-485,
J2 = underglow/GND, and the nine INT connectors are J3–J11. Physical order on
the board is J11 → J3 at 7mm pitch, left to right, so the *connector* numbers
descend across the board while the per-line D/R/C numbers ascend with panel
order (UL = D1/R6/C3 … DR = D9/R14/C11).

**Color column:** the stock SMX per-panel map. The chosen INT cable (RVSP
twisted pair) is only available in a single color, so these are **end markers —
colored or printed heat-shrink at both cable ends — not conductor insulation
colors.** Slot↔panel agreement is still not assumed: the master learns the real
mapping with the `'I'` identify pulse and reports mismatches
(`docs/RS485_PROTOCOL.md`).

**Per line — two-stage ESD/EMI protection + filter (added 2026-07-24, resolves
review F3/F4):**
1. Off its connector the line first meets an **SMAJ5.0A unidirectional TVS to
   GND** (Dx,
   cathode on the line / anode to GND, own GND via — negative transients
   forward-conduct at ~0.7V). This is the *entry* node.
2. Then a **330Ω series R** (R6–R14) into the Teensy-side node `INT_xx`, which
   carries the **10k pull-up** (RN1, bussed — deliberately **not stiff**, for the
   INT-into-dead-panel case) + a **1nF C0G cap** (C3–C11) to GND + the Teensy pin.

The series R limits residual ESD current into the Teensy's internal clamp
(~16mA at the ~9.2V TVS clamp — this is what makes a 5V-standoff TVS safe on a
non-5V-tolerant pin) and forms a ~330ns RC low-pass with the cap, killing
ESD/EMI without meaningful press latency (the INT edge is the **sole** game
input — no FSR veto — so integrity matters; see `docs/USB_PROTOCOL.md`). Pull-up
sits on the Teensy-side node so the pin is defined HIGH locally and stays safe
if the series R ever opens. **Per-line refs:** DR=R14/C11, D=R13/C10, DL=R12/C9,
R=R11/C8, C=R10/C7, L=R9/C6, UR=R8/C5, U=R7/C4, UL=R6/C3. TVS orientation
matters at assembly. (The discrete TVS-per-line approach replaced 3× SRV05-4
arrays 2026-07-22.)

### INT cabling (twisted pair, adopted 2026-07-26)

Each INT line is a **twisted pair: signal + its own GND return**, rather than a
single conductor returning through the shared power ground network. This was
previously listed as a reserved mitigation to hold in case the bench showed
spurious triggers; it was adopted up front instead, since the panel side already
had the GND position provisioned (the panel's J214 pin 2, 2026-07-24) and the master side only
needed a connector change.

- **Master side:** the 9-position pluggable Euroblock was replaced by **nine
  JST XH 2-pin vertical headers** (B2B-XH-A). Nine discrete 5.08mm screw blocks
  were rejected — ~92mm of board edge against a 77.5mm board. XH is top-entry,
  so it needs no board edge at all and the nine fit in roughly the footprint the
  Euroblock vacated (~42 × 13.5mm vs 45.72 × 12.0mm).
- **Why XH over a terminal block:** it is keyed, so signal and GND cannot be
  swapped at assembly. On a screw terminal that swap ties INT to GND and reads
  as a permanently stuck press — an expensive bug to chase at bring-up. Using XH
  here while the panel FSR connectors stay PH also means an FSR lead physically
  cannot be plugged into an INT header.
- **Cost:** the harness loses single-action bulk disconnect (nine plugs instead
  of one block). Accepted — per-panel disconnect is more useful for servicing a
  single dead panel, and the master retains no other reason to detach all nine
  at once.
- The dedicated return does form a loop against the shared power ground, but the
  twisting collapses the loop *area*, which is what governs pickup, and the
  current involved is microamps. Standard practice, not a compromise.

## Parts (refs → identity)

| Ref(s) | Part | Notes |
|--------|------|-------|
| U1 | THVD1429DR (SOIC-8) | same part as panels; VCC +3.3VDC, C1 100nF |
| U2 | Teensy 4.0 (PJRC 15583), socketed | 2× PPPC141LFBN-RC 14-pos female headers |
| U3 | SN74AHCT1G125DBVR (SOT-23-5) | underglow shifter (single gate, swapped from quad SN74AHCT125N DIP 2026-08-03), VCC +5VDC_USB, C2 100nF; A ← GPIO11, Y → R5 330R → J2.1; OE̅ → GND |
| D1–D9 | SMAJ5.0A TVS (DO-214AC) | one per INT line, entry-node ESD clamp — see table above |
| R6–R14 | 330R 0805 | INT series R per line (ESD limit + RC filter into Teensy) |
| C3–C11 | 1nF C0G 0805 | INT filter cap per line, Teensy-side node → GND |
| R4 | 10k 0805 | `UNDERGLOW_DATA` pull-down (defines U3 A input LOW at boot) |
| R3 | 120R | RS-485 termination, always fitted (master is always a bus end — no switch) |
| R1/R2 | 390R 1% — **DNP** | RS-485 failsafe bias (+3.3VDC→RS485+, RS485−→GND). THVD1429's integrated open/short/idle failsafe makes them unnecessary; footprints exist so bias can be added at the one correct bus point if the bench ever disagrees (≈236mV across the 60Ω loaded bus) |
| RN1 | Bourns 4610**X**-101-103LF (SIP-10, 10k ×9 bussed, LCSC C840655) | pin 1 common → +3.3VDC |
| SW1 | DORABO DS-3P-BU (DIP-3, LCSC C46595747) | player ID 0–7 to GND, internal pull-ups |
| J1 | Micro-Fit 43650-0300 (RS-485 OUT) | A=pin 1, B=pin 2, **pin 3 = cable shield, tied directly to GND here** (see "RS-485 shield" below) — **matches the panel's J204/J207 exactly** so the cable is straight-through |
| J3–J11 | JST B2B-XH-A 2-pos 2.5mm vertical THT (LCSC C158012) | one per INT line; pin 1 = INT signal, pin 2 = dedicated GND return. Mating half is XHP-2 housing (C144401) + SXH-001T-P0.6N contacts (C385122), 22–26 AWG. Symbol is generic `Connector_Generic:Conn_01x02`; there is no JST-specific symbol in KiCad. Replaced the 9-pos Euroblock 2026-07-26 |
| J2 | KANGNEX WJ500V-5.08-2P 2-pos screw terminal (LCSC C8465) | pin 1 = underglow DATA (from R5, the 330R series element — R3 is the RS-485 termination), pin 2 = **mandatory GND tie** to the PSU ground stud. DATA position may sit empty if underglow unused |
| TP1–TP10 | THT probe holes | TP1 RS-485 RX / TP2 DE / TP3 RS-485 TX / TP4 +5VDC_USB / TP5 GND / TP6 +3.3VDC / TP7 RS485+ / TP8 RS485− / TP9 underglow 3.3V side / TP10 underglow 5V side |
| H1–H4 | M3 mounting holes | |

## Layout (as built)

- **4 layers: Sig+Pwr / GND / GND / Sig+Pwr** (JLC04161H-7628 stackup). Each
  outer layer references its adjacent GND plane; no power plane needed —
  logic-only currents (single-digit mA on 3.3V). The +3.3V net is routed on
  the outer layers. In1/In2 stitched liberally (same net), concentrated at J3,
  U1, and the Teensy.
- **RS-485 pair `/RS485+` `/RS485-` (as measured 2026-07-24):** W=**0.15mm**,
  gap **0.2mm**, F.Cu only, **zero vias**, both legs **45.39mm — 0.000mm
  skew**. That geometry gives ~119Ω against a 120Ω target on this stackup
  (Hammerstad-Jensen, cross-checked vs IPC-2141, ±10% for an
  uncontrolled-impedance order). Same stackup and therefore the same target
  geometry as the panel — see `docs/DUAL_PANEL.md` for the derivation and the
  history of the earlier default-stackup miscalculation.
  - ~69% of the run is coupled at 0.2mm; the remaining ~14mm fans out
    progressively to reach pads that are simply farther apart than the pair
    pitch (U1 pins 6/7, J1 Micro-Fit, R3 termination, TP7/TP8 — five pad
    landings on the + net). That fan-out is unavoidable and inconsequential at
    1 Mbps. **Do not "fix" it.**
  - Master inner layers are **both GND** (`In1.Cu "GND_1"`, `In2.Cu "GND_2"`),
    unlike the panel's GND/power split — so there is no reference-plane-type
    change anywhere on this board, and any future via transition is fully
    solved by a nearby GND stitching via.
- No SMD parts under the Teensy socket; hot-air approach room around D1–D9
  and the 0805s (electrically they belong at the INT connectors, but clear of
  the socket — this constraint predates the Euroblock→JST XH swap and the
  low-profile XH bodies only relax it).
- Target ~80×60mm; enclosure to be modeled from the KiCad 3D export once
  boards are in hand.

## Deliberately absent (do not re-add)

- Any 12V input, distribution, or sensing — the master never sees 12V.
- Underglow 5V-in from the stock Daygreen converter (Teensy USB rail already
  provides shifter VCC) and an underglow presence-sense pin — UI/config
  gating for now; the harness teardown may reopen it (a 12V-sense divider like
  the panel's is the known upgrade path).
- ~~GND position on the INT Euroblock~~ — **reversed 2026-07-26.** The Euroblock
  is gone and every INT connector now has a dedicated GND pin; the return no
  longer rides the power ground network. See "INT cabling" above.
- RS-485 termination switch — master end is always terminated (R3 fixed).

## RS-485 shield (adopted 2026-07-26)

The RS-485 cable is shielded (RVSP foil + braid), and the third Micro-Fit
position — previously left deliberately unpopulated — now carries that shield.
This **reverses the earlier "drain floating at both ends" decision.** Micro-Fit
2- vs 3-circuit keying still prevents 12V from ever reaching a transceiver, and
pin 3 carries shield only: no rail, no signal.

Topology is **hybrid grounding**, one continuous shield from master to panel 8:

- **Master (here):** J1 pin 3 → `GND`, plain trace, no parts. This is the
  **single DC reference for the entire shield network.** Without it the shield
  floats and the scheme is worse than not having one.
- **Each panel:** `RS485_Shield` runs J204 pad 3 → J207 pad 3 as a pass-through
  with **no local GND tie**, plus 100nF (C202) ‖ 1MΩ (R202) to GND near J204.
- **Panel 8:** far end, shield simply terminates.

The per-panel caps are not optional decoration: ~6–8m of foil grounded at one
end only resonates near 10 MHz, inside the harmonic content of 1 Mbps edges.
The 100nF grounds the shield at RF while blocking DC, so no ground loop forms
against the 12V ground network; the 1MΩ bleeds tribocharge (rubber-soled shoes
on the panels charge the pad — the same reasoning behind the panel's USB ESD
array). Keep the cap's GND via at the pad: series inductance there directly
undoes it.

Each cable segment lands the shield at **both** its connectors — the pass-through
traces make it electrically continuous end to end. "Grounded at one end" refers
to the whole network, not to each segment. If the cable has no drain wire, gather
the braid into a pigtail, solder a short lead to it, heatshrink the joint, and
crimp that lead — a stray braid strand bridging a signal pin is the classic
failure here.

**Not applied to the INT harness:** JST XH has only two positions, so if the INT
cable arrives shielded the shield is trimmed and heatshrunk at both ends. Do
**not** bond it to the INT GND conductor — that is a signal return, and paralleling
a shield across it re-creates the loop area the twisting exists to remove.

## Open items

- Underglow connector final form — the J2 screw terminal is the interim
  decision; the harness splice point (stock leads crimp into a 12-pin Dupont
  at the old MCU) is decided at teardown and may change it. A GND position
  adjacent to DATA preserves the option of a paired/twisted return wire.
- ~~Master INT filter caps~~ **DONE 2026-07-24** — full two-stage TVS + 330Ω
  series R (R6–R14) + 1nF RC (C3–C11) per INT line is now on the board (see the
  INT block section above).
- Firmware: USB HID to the PC is the one major master-side piece not started
  (RS-485 + INT handling already proven on the prototype). Note the master-side
  **glitch-qualify window** requirement (µs-scale, hidden in USB dead-time,
  never average on master) — see `docs/USB_PROTOCOL.md`.
