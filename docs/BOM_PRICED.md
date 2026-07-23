# Priced BOM — Kraken Pad (2 masters + 20 panels)

Snapshot of the DigiKey cart **2026-07-22**, prices in **CAD**. Built for shopping around —
the **Unit** column is what to compare against other vendors. Quantities cover the full build
(2 master boards + 20 panel boards, some lines carry spares).

> Panel **SMD** parts are not here — those go to JLCPCB via LCSC (PCBA). This list is the
> DigiKey / hand-solder side only.

## Master PCB electronics (x2 boards)

| Part | MPN | DigiKey PN | Qty | Unit $ | Ext $ | Used on |
|------|-----|-----------|-----|--------|-------|---------|
| MCU | 15583 | 1568-15583-ND | 2 | 36.260 | 72.52 | U1 Teensy 4.0 |
| RS-485 transceiver | THVD1429DR | 296-THVD1429DRCT-ND | 2 | 7.150 | 14.30 | U2 |
| Level shifter | SN74AHCT125N | 296-4655-5-ND | 2 | 1.520 | 3.04 | U3 |
| Resistor array 10k x9 | 4610M-101-103LF | 4610M-101-103LF-ND | 2 | 1.330 | 2.66 | RN1 |
| DIP switch 3-pos | DS01C-254-S-03BE | 2223-DS01C-254-S-03BE-ND | 2 | 1.010 | 2.02 | SW1 player ID |
| Euro header 9-pos | 0395316009 | WM25993-ND | 2 | 5.400 | 10.80 | J2 INT in (header) |
| Euro plug 9-pos | 0395337009 | WM25575-ND | 2 | 13.980 | 27.96 | J2 INT in (plug) |
| Cap 100nF 0805 | C0805F104K1RACAUTO | 399-C0805F104K1RACAUTOCT-ND | 4 | 0.360 | 1.44 | C1,C2 |
| Res 120R 0805 | RC0805FR-07120RL | 311-120CRCT-ND | 2 | 0.160 | 0.32 | R1 term |
| Res 1k 0805 | CRCW08051K00FKEA | 541-1.00KCCT-ND | 2 | 0.160 | 0.32 | R2 LED |
| Res 330R 0805 | RC0805FR-07330RL | 311-330CRCT-ND | 2 | 0.160 | 0.32 | R3 underglow |
| LED blue 0805 | 150080BS75000 | 732-4982-1-ND | 2 | 0.300 | 0.60 | D1 status |
| TVS 5V SMA | SMAJ5.0A | SMAJ5.0ALFCT-ND | 18 | 0.431 | 7.76 | D2-D10 INT ESD |
| Teensy socket 14-pos | PPPC141LFBN-RC | S7047-ND | 4 | 1.470 | 5.88 | U1 socket (2/board) |
| | | | | **subtotal** | **149.94** | |

## Panel PCB through-hole (x20 boards)  + shared

| Part | MPN | DigiKey PN | Qty | Unit $ | Ext $ | Used on |
|------|-----|-----------|-----|--------|-------|---------|
| Micro-Fit 2pin RA | 0436500200 | WM1860-ND | 40 | 1.370 | 54.80 | J5,J11 12V |
| Micro-Fit 3pin RA | 0436500300 | WM1861-ND | 42 | 1.551 | 65.14 | J8,J10 (+master J1) |
| USB-C receptacle | USB4085-GF-A | 2073-USB4085-GF-ACT-ND | 20 | 1.158 | 23.16 | J1 |
| Terminal 2-pos MRR52 | MRR522-5.08-V | 5245-MRR522-5.08-V-ND | 25 | 0.674 | 16.85 | J9 (+master J4, +spare) |
| DIP switch 4-pos | DS01C-254-S-04BE | 2223-DS01C-254-S-04BE-ND | 20 | 0.902 | 18.04 | SW1 panel ID |
| Slide switch DPDT | EG2201A | EG1905-ND | 20 | 1.529 | 30.58 | SW3 RS-485 term |
| Pin header 3-pos | 61300311121 | 732-5316-ND | 25 | 0.141 | 3.53 | J2 SWD |
| | | | | **subtotal** | **212.10** | |

## Harness (mating connectors + crimps)

| Part | MPN | DigiKey PN | Qty | Unit $ | Ext $ | Used on |
|------|-----|-----------|-----|--------|-------|---------|
| Micro-Fit recept hsg 2pos | 0436450200 | WM1845-ND | 30 | 0.470 | 14.10 | 12V column cables |
| Micro-Fit recept hsg 3pos | 0436450300 | WM1846-ND | 36 | 0.496 | 17.87 | RS-485 cables |
| Micro-Fit crimp 20-24AWG | 0430300001 | WM1837CT-ND | 200 | 0.091 | 18.30 | all Micro-Fit |
| | | | | **subtotal** | **50.27** | |

## Wire (PLACEHOLDER lengths - refine later)

| Part | MPN | DigiKey PN | Qty | Unit $ | Ext $ | Used on |
|------|-----|-----------|-----|--------|-------|---------|
| 2C 20AWG shielded | 5400FE 008500 | 135-5400FE008500-DS-ND | 10 | 1.446 | 14.46 | 12V power (ft) |
| 2C 22AWG twisted RS-485 | 8761 06010000 | 135-876106010000-DS-ND | 10 | 4.157 | 41.57 | RS-485 (ft) |
| 24AWG stranded x9 colors | 3xxx-24-... | (9 lines) | 90 | ~0.10-1.25 | 91.52 | INT wires (ft) |
| | | | | **subtotal** | **147.55** | |

## Totals

| Section | CAD |
|---------|-----|
| Master PCB electronics | 149.94 |
| Panel PCB through-hole | 212.10 |
| Harness | 50.27 |
| Wire | 147.55 |
| **GRAND TOTAL** | **559.86** |

Wire is ~26% of the order and is all placeholder length — the biggest single lever for cost-shopping (a multi-colour 24AWG assortment kit likely beats 9 separate spools).

## Not on this order — have on hand or source elsewhere

| Item | Status | Notes |
|------|--------|-------|
| FSR sensors | **reuse stock SMX** | Interlink FSR 408 model; may test alternatives later |
| FSR JST connectors (B2B-PH-K-S) | **NEEDS A SOURCE** | 80 needed (4/panel); out of stock at DigiKey |
| 18 AWG stranded (underglow/GND-tie) | have at home | |
| Spade / fork lugs (PSU ends + master GND tie) | have at home | |
| M3 mounting hardware (H1-H4 + panel holes) | have at home | screws/standoffs/nuts |
| Master enclosure | future | 3D-print; model from KiCad 3D export -> Fusion 360 once boards are in hand |

## Notes for cost-shopping

- **Teensy 4.0 ($36 ea)** is the single biggest line and rarely discounted — PJRC direct or
  Adafruit/SparkFun are the usual sources; watch for clones only if you accept the risk.
- **Micro-Fit connectors + crimps** (housings, headers, crimps) add up across 20 panels —
  AliExpress/LCSC often much cheaper for Molex-compatible, at some QC risk on crimps.
- **RS-485 cable (Belden 8761, $4.16/ft)** is premium; any 22-24AWG shielded twisted pair works.
- **9-colour INT wire**: an assortment kit beats nine spool minimums.
- The **Micro-Fit 3pin (WM1861) was on backorder** at cart time — check stock before ordering.

