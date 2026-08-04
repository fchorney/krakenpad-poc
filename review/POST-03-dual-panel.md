# Draft post — panel, two-board design (carrier + brain)

Two boards, so this is arguably two review requests. The subreddit's limit is
one review *per board* per day, and they mate mechanically, so posting them
together as one request is reasonable — just be explicit in the title and the
image captions about which board each image shows.

**Title**

> Review Request: two-board dance-pad panel — RP2040 "brain" board plus LED/IO carrier, 32-pin board-to-board interface, RS-485, 25× WS2815, 4× FSR

**Images** (upload each separately with its own caption)

| Caption | File |
|---|---|
| Carrier — schematic | `03-dual-carrier/01-schematic-1-carrier.png` |
| Carrier — 2D PCB top | `03-dual-carrier/02-2d-top.png` |
| Carrier — 2D PCB inner 1 | `03-dual-carrier/03-2d-in1.png` |
| Carrier — 2D PCB inner 2 | `03-dual-carrier/04-2d-in2.png` |
| Carrier — 2D PCB bottom, mirrored | `03-dual-carrier/05-2d-bottom-mirrored.png` |
| Carrier — 2D PCB top, no pour | `03-dual-carrier/07-2d-top-no-pour.png` |
| Carrier — 2D PCB bottom, no pour, mirrored | `03-dual-carrier/10-2d-bottom-no-pour-mirrored.png` |
| Carrier — 3D top, plan view | `03-dual-carrier/11-3d-top.png` |
| Carrier — 3D bottom, plan view | `03-dual-carrier/12-3d-bottom.png` |
| Brain — schematic | `04-dual-brain/01-schematic-1-brain.png` |
| Brain — 2D PCB top | `04-dual-brain/02-2d-top.png` |
| Brain — 2D PCB inner 1 | `04-dual-brain/03-2d-in1.png` |
| Brain — 2D PCB inner 2 | `04-dual-brain/04-2d-in2.png` |
| Brain — 2D PCB bottom, mirrored | `04-dual-brain/05-2d-bottom-mirrored.png` |
| Brain — 2D PCB top, no pour | `04-dual-brain/07-2d-top-no-pour.png` |
| Brain — 2D PCB bottom, no pour, mirrored | `04-dual-brain/10-2d-bottom-no-pour-mirrored.png` |
| Brain — 3D top, plan view | `04-dual-brain/11-3d-top.png` |
| Brain — 3D bottom, plan view | `04-dual-brain/12-3d-bottom.png` |

(Silkscreen-only and inner-layer no-pour images are in the folders too if a
reviewer wants them.)

**Body**

The panel is split across two boards: a large carrier that holds the LEDs, sensors, connectors and switches,
and a small brain that holds the MCU, flash, USB, regulators and the RS-485
transceiver. Nine of these go in a dance pad; all nine are identical.

- **Carrier**: 127 × 127 mm core, 139.3 mm across the connector tabs, 4 layers.
  25× WS2815 (12 V LEDs, 5 V data), 4× FSR on JST PH 2.0 mm leads, 12 V in/out
  and RS-485 in/out on 3 mm pitch latching connectors, interrupt out on a
  2-position 5.08 mm screw terminal, 4-position panel-ID DIP, DPDT bus
  termination switch, 470 µF bulk, SWD header.
- **Brain**: 70.9 × 62.6 mm, 4 layers. RP2040 (QFN-56), 12 MHz crystal,
  W25Q32JV 4 MB QSPI flash, edge-mounted USB-C for bench flashing with
  USBLC6-2SC6 ESD protection, BOOTSEL, THVD1429 RS-485 transceiver,
  SN74AHCT125 level shifter, AMS1117-5.0 → AP7361C-33ER linear cascade off the
  12 V bus, LM66200 ideal-diode mux ORing USB VBUS against the 5 V rail
  (PMEG3015EH pair kept as DNP fallback footprints).
- **Interface**: 4× 1×08 headers/sockets on 2.54 mm (J210–J213 mating J301–J304), 32 pins total, carrying
  roughly two dozen signals plus power. The brain sits below the carrier in the
  pad cavity.
- Both boards are drawn in one KiCad project (one hierarchy, one netlist, two
  outlines in one board file) and get ordered as a single panel, so the
  interface is ERC-checked rather than hand-maintained across two projects.

What I would most like eyes on:

1. The board-to-board interface: 32 pins of 2.54 mm header/socket under a device
   that gets **stomped on by a human being**. Pin assignment, mechanical
   retention, whether this connector family is the wrong tool here.
2. Analog return paths now that the ADC inputs cross a connector — the FSR
   dividers are on the carrier, the ADC is on the brain.
3. Power: 12 V bulk and the linear cascade sit on the brain, but the LED current
   is all on the carrier. Plane and decoupling strategy across the split.
4. RS-485 and the shield handling — the connectors are on the carrier, the
   transceiver on the brain.
5. Layout and silkscreen hygiene on both boards.

The carrier's board-to-board headers (J210–J213) are bottom-mounted, since the
brain hangs underneath — they show as plated holes in the carrier's top view and
as headers in its bottom view. Their mating sockets on the brain are J301–J304.

Reference designators are numbered in per-sheet blocks — carrier 2xx, brain 3xx —
so a designator says which board the part is on.
