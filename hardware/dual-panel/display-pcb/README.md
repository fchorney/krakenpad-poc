# display-pcb — LED + IO carrier (exploration)

Sits where `hardware/panel-pcb/` sits today: 127×127mm, same four standoffs, same
connector tabs, same frame cavity underneath — the brain sockets into that cavity
from below.

**Carries:** 25 WS2815s + their pin-1 caps · FSR front end (J3/J4/J6/J7 +
dividers) · power entry (J5/J11) + 470µF bulk · RS-485 in/out (J8/J10) +
termination switch and its 120Ω · INT screw terminal + TVS and series R ·
panel-ID DIP · USB-C + CC pull-downs + USBLC6 ESD array · BOOTSEL button · SWD
header · debug LED · board outline, mounting holes, logo.

**Does NOT carry:** anything in `../brain-pcb`.

Design intent: protection at the ports (a plug event or ESD strike clamps here,
upstream of the board-to-board connector, and never reaches the brain), and a
stackup simple enough to attempt in **2 layers** — LED chain + 12V distribution +
GND pour. See `../README.md` for the interface, the open gates and the rationale.
