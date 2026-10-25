# Rev 2 candidates

Board changes that would go into the next fab revision. **The rev-1 boards in
use are frozen** (ordered 2026-08-18, tag `rev1-fab`): nothing here changes them.
Where it matters, an entry says what rev 1 needs instead (usually nothing).

Nothing on this list is decided unless the entry says so. Each entry records
when and where it was found, so it can be weighed with its original evidence.
`CLAUDE.md` carries a one-line index of this list.

## Candidates

1. **External TVS on the RS-485 A/B pair** (decided 2026-08-18, purely additive —
   the bus has no external TVS; D201 is on INT).
2. **Master: raise the ground-pour clearance above 0.09 mm** (found 2026-08-22,
   `docs/rev1-order/CAM_REVIEW.md` §4). Some pour-to-trace gaps sit *exactly* on JLCPCB's
   3.5 mil floor, so after their +15 µm etch compensation they would have gone
   sub-spec. JLC's CAM silently opened those gaps to 180 µm rather than querying
   — verified non-destructive (component counts identical, no slivers), and rev 1
   is fine. But the design should not depend on a vendor's CAM being generous.
   **Master only; the panel shows none of it.**
3. **Brain: VBUS sense on a spare GPIO** (found at the bench 2026-09-07). The
   brain **back-drives its own VBUS to 2.78 V** whenever it runs on 12V: the
   RP2040 asserts the D+ pull-up (1.5 k to 3.3 V) as soon as the USB stack
   inits, and `U305`'s I/O→VBUS ESD diode carries that onto the VBUS net,
   which has **no bleeder** (net = `J305` VBUS + `U304` IN1 + `U305` only).
   2.78 V is above vSafe0V, so **a USB-C host refuses to attach: you cannot
   plug a laptop into a panel that is already powered.** Measured, not
   inferred — D+ 3.3 V, VBUS 2.78 V, one diode drop apart. **Rev 1 needs no
   rework: connect USB *before* 12V.** Fix is a VBUS divider into one of the
   free GPIOs (2, 3, 5–9, 12–15, 23–25) so firmware can `tud_connect()` on
   real VBUS instead of the SDK's blanket `VBUS_DETECT_OVERRIDE`; it drains
   the node as a side effect. ⚠ **A plain bleeder is the wrong fix** — against
   a 1.5 k pull-up it must be <600 Ω and then burns ~8 mA whenever USB is live.
4. **Brain: an on-board indicator LED** (raised at the bench 2026-10-02). An
   unmated brain shows nothing to say it is alive: the only indicator is the
   carrier's `D202` debug LED (GPIO16 via the interface, `R204` 1k). Proposed:
   mirror the debug LED onto the brain. ⚠ `D202` is **firmware-driven**, so a
   copy of it stays dark on a blank board, in BOOTSEL, or under hung firmware,
   which are the states where "is it powered?" matters most. If the goal is
   *power*, put an LED on `+3.3VDC`. A debug mirror and a power LED answer
   different questions; the brain could carry either or both. Not decided.
5. **Test-point labels on both sides of the board** (raised at the bench
   2026-10-02). Every test point is a THT pad, so it can be probed from
   either side, but only the refdes is on silk, and only on **F.SilkS**. The
   brain mounts **F.Cu toward the carrier**, so when the boards are mated the
   side you can see has no labels at all. Proposed: label on B.SilkS too, and
   label by **signal** (`RUN`, `DE`, `+5V`), not just `TP306`. ⚠ The
   footprint **Value** fields can't be used as the source text as they are:
   9 carrier test points carry a wrong Value (e.g. `TP201` says `+12VDC` but
   is `TERM_SENSE`; `TP202`/`TP204` say `GND` but are `RS485±`). The
   test-point list in `docs/DUAL_PANEL.md` is taken from the net (corrected
   2026-10-02). Take the label text from the net too.
6. **More test points** (raised 2026-10-02, list not final). **VBUS** first:
   there is none on the brain, and it is the net behind #3. Pairs naturally
   with #3's divider. Suggested, not decided: the **AMS1117-5.0 output
   before the `U304` mux**. With USB present the mux selects VBUS, so `U303`
   can't be metered from `TP302` (bring-up had to run 12V-only to test it).
   Also **`SENSE_12V`**, which was behind the GPIO17 pull-down trap.
7. **Probe hooks in place of the THT pad test points** (raised 2026-10-02,
   user still considering). A board constraint to check before choosing:
   the brain's F side faces the carrier, so a hook on that side would sit in
   the inter-board gap. Hooks there would go on **B**, which also decides
   where #5's labels go.
8. **Brain: `J305` (GCT USB4085-GF-A) pins barely protrude** (raised at the
   bench 2026-10-15). The signal pins and the shell/ground lugs only just
   come through the board. They solder fine, so **rev 1 needs nothing**, but
   there is very little to wet a fillet onto. Options: a USB-C part with
   longer TH legs, or **SMT signals + TH shell lugs**. ⚠ The latter
   reverses an earlier rejection (GCT USB4105 was turned down for its SMT
   signal pins), but the written requirement was always "through-hole **or
   TH-reinforced**" — the TH lugs take the plug/unplug load, so a hybrid
   part meets it. **Bench estimate by eye** (too small to measure with
   calipers), board **1.57 mm**: signal pins ~0.1 mm past the holes, the two
   rear shell lugs ~0.5 mm, and the **two front lugs do not reach the far
   side at all**. Before choosing a rev-2 part, check what board thickness
   its datasheet assumes. Not decided.
9. **Both boards: SMD wherever possible, and wash-safe parts** (raised
   2026-10-18, after hand-fitting the TH parts on eight panels). Prefer SMD
   and look for SMD replacements for TH parts where one makes sense. Prefer
   parts that tolerate an IPA wash. Going fully SMD would mostly make the
   wash question disappear, because JLC's assembly line and its cleaning
   step would then handle every part. The TH parts in rev 1, from the PCB
   files (2026-10-18):
   - **Carrier:** `J201`–`J203`, `J206` JST PH (FSR); `J204`/`J207` and
     `J205`/`J208` Micro-Fit 3.0, horizontal; `J209` SWD header;
     `J210`–`J213` 1×8 headers (brain interface); `J214` screw terminal
     (INT); `SW201` ID DIP; `SW202` termination slide switch; 30 test
     points.
   - **Brain:** `J301`–`J304` 1×8 sockets (brain interface); `J305` USB-C.
   - **Master:** `J1` Micro-Fit 3.0; `J2` screw terminal; `J3`–`J11` JST
     XH; `RN1` SIP-10 resistor network; `SW1` player-ID DIP; `U2` Teensy
     socket; 10 test points.

   Overlaps with other candidates: #5–#7 (test points as SMD pads or
   hooks), #8 (`J305`), and the brain interface, which is the largest
   group of joints (64 per panel). Some of these exist as TH on purpose,
   so each swap re-opens a recorded choice: the mechanical-load reasoning
   for the cable connectors and for `J305`, and the socketed Teensy.
   Wash-safety is per-part datasheet work, not yet done; switches and
   unsealed connectors are the usual suspects. Not decided.
10. **Carrier: automatic termination via chain discovery** (raised
    2026-10-18, **parked, something to think about only**). Today the user
    sets `SW202` on whichever panel is physically last, and nothing on the
    boards can verify the chain order: IN and OUT are straight copper, so
    every panel looks the same from the master. Idea: a switch in series
    between each carrier's RS-485 IN and OUT (A and B), plus a
    GPIO-controlled switch in series with `R201` in place of `SW202`. The
    master closes segments one at a time, panels appear in physical order,
    and it tells the last one to terminate. That would make the
    placement check (`docs/MASTER_BRINGUP.md` → "Termination check") a
    measurement instead of an assumption. No connector change.
    ⚠ A plain 3.3 V analog switch is the wrong part: it only passes
    signals within its rails, and RS-485 is specified for −7 to +12 V
    common mode. Candidates are fault-protected/wide-range analog switches
    or PhotoMOS relays. **Consequences:** a series switch that fails open,
    or is open while a panel is unpowered, crashed or in the bootloader,
    cuts off every panel downstream, which today's passive pass-through
    never does. So it would have to be normally closed (e.g. a form-B
    PhotoMOS), and the bootloader must leave it closed. On-resistance and
    capacitance add up over eight hops. **User's view (2026-10-18): the
    consequences feel like more trouble than they're worth; maybe worth a
    deeper look when there is time, not now.** Context: on the bench, the
    bus ran with zero errors terminated, unterminated, mid-chain and
    doubled (`docs/BRINGUP_LOG.md`, master #2), so the payoff is
    convenience and margin, not a working bus. Not decided.
11. **Master: run from the pad's 12V, so it comes up with the panels**
    (raised 2026-10-22, **something to think about, not decided**). Today the
    master runs only from USB, so with the PC off or unplugged the panels
    have 12V and no master: no LED frames, no underglow, no config push. The
    stock pad powers its MCU from the pad's 5V rail
    (`stock-smx/harness/mcu-interface.yml`), so the whole pad comes up on
    12V alone. For us: a 12V feed from the Wago fan-out to the master, an
    on-board regulator, and the Teensy 4.0's VIN–VUSB link cut so the pad's
    supply and the PC's VBUS never feed each other. The master's own idle
    lighting and the underglow would then run with the PC off.
    **Consequences:** the master is on whenever 12V is; 12V reaches a board
    that has none today (the master is outside the 12V path), which brings
    this board into the "no board-level power protection" notes below; more
    parts and a harness change. On rev 1 the panels answer "is it powered?"
    themselves: a no-master standby breath (2026-10-22, `docs/PANEL_CONFIG.md`
    → "LED modes"), which with this change would only show on a fault.
    **User's view (2026-10-22): extra parts and complexity, but maybe worth
    it; not deciding now.**
12. **Master: a 3-pin debug header, GND / RX1 / TX1** (raised 2026-10-25, the
    user). GPIO 0/1 are Serial1, the last spare hardware UART, kept free on
    purpose (`docs/MASTER_PCB.md` pin map) but not brought out. A header gives
    the master a console that doesn't depend on USB: readable while a game
    owns the PC and never opens the CDC port, and across USB re-enumeration.
    Next to the Teensy's pin-0 corner, where GND, 0 and 1 already sit
    together. On rev 1 the same three can be wired by hand to the Teensy's
    own pins (it is socketed, so the PCB isn't touched); a series resistor
    (~1 kΩ) in each signal line keeps a powered logger from back-powering an
    unpowered master through its I/O pins.
13. **Master: test points on all nine INT lines** (raised 2026-10-25, the
    user). The master has TP1–TP10 (RS-485, DE, rails, underglow) and none on
    INT. The INT release at DR was slow (67 µs) and crossed mid-rail 19–41
    times (`docs/BRINGUP_LOG.md`, scope 2026-10-23), the likely source of the
    master's `glitches` count, but it hasn't been seen **at the master's
    pin**, because there is nowhere to probe there. Put them on the
    Teensy-side node `INT_xx` (after the 330 Ω, where the 1 nF and the pull-up
    sit; `docs/MASTER_PCB.md` → INT protection), and label them by panel
    (`UL` … `DR`, as `J3`–`J11`). Pairs with #5 and #7.

## Dropped

- ~~RS-485 DE pull-down~~ **DROPPED 2026-09-07**: `TP306` measured 35 mV on a
  blank board, so the RP2040's internal pull-down does park the THVD1450 in
  receive. See `docs/BRINGUP_LOG.md`.

## Decided for rev 1, with notes for a revisit

These are closed decisions, not open candidates. They are listed so the notes
are found when rev 2 is planned.

- **No board-level power-input protection** (reverse polarity, per-panel fuse).
  **DECIDED 2026-08-17** for rev 1: the mitigations are procedural (keyed XT30
  and Micro-Fit, the T8A trunk fuse, engraved fan-out, metering before first
  power-on). `docs/rev1-order/POWER_PROTECTION.md` ends with "Notes for a rev-B
  revisit": a trunk-level P-FET was never evaluated, and if the layout is open
  anyway, F201/Q201 can go in as DNP footprints at no fab cost.
