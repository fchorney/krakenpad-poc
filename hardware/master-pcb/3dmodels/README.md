# Third-party 3D models — master PCB

| File | Part | Source | License |
|------|------|--------|---------|
| `Teensy_4.0_Assembly.STEP` | PJRC Teensy 4.0 | [XenGi/teensy.pretty](https://github.com/XenGi/teensy.pretty) | MIT (see `LICENSE-XenGi-teensy.pretty.txt`) |

The Teensy 4.0 **symbol** (`master-pcb.kicad_sym`) and **footprint**
(`master-pcb.pretty/Teensy40_Socketed.kicad_mod`) come from the same upstream
project ([XenGi/teensy_library](https://github.com/XenGi/teensy_library) and
[XenGi/teensy.pretty](https://github.com/XenGi/teensy.pretty), both MIT).

The footprint is **locally modified**: upstream pads 35–44 (Teensy bottom-side
pins 24–33) were removed. This design does not use those pins, and as
through-holes they sat in a 2×5 grid directly under the socket, consuming
routing area. Pads 1–34 are unchanged from upstream.
