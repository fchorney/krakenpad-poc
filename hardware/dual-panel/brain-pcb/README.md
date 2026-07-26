# brain-pcb — MCU / power / comms board (exploration)

Sockets into `../display-pcb` from below, sitting in the frame cavity (~95–105mm
square, depth unmeasured — the one hard gate). Two M3 screws into the cavity's
corner bosses take all mechanical load; the socket carries none.

**Carries:** RP2040 + decoupling · QSPI flash + its 1k/10k pair · 12MHz crystal +
loads · THVD1429 RS-485 transceiver · SN74AHCT125 level shifter · AMS1117-5.0 →
AP7361C-3.3 cascade · LM66200 power-OR (+ the DNP Schottky rescue) · 27Ω USB
series resistors · 12V sense divider + clamp.

**Does NOT carry:** connectors, switches, LEDs, or any port protection — those
live on the carrier. Notably **USB-C, SWD and BOOTSEL are all on the carrier**;
`USB_BOOT` crosses the connector behind the 1kΩ that already isolates it from
QSPI chip-select.

Target ~60×50mm. This is the board that gets re-spun if bring-up finds a problem,
which is the whole point of the split — small area means a 6-layer stackup
(sig/GND/sig/GND/pwr/sig) costs about what 4-layer costs on the full panel today.
