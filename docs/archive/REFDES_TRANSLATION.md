# Reference-designator translation — `panel-pcb` → `dual-panel`

`hardware/panel-pcb` was deleted 2026-08-04 (git history, last present at
`1b41d1c`). Its reference designators appear throughout git history, the archived
reviews, older memory notes and any external review thread from before that date.
This table translates them.

**Derived, not hand-written.** Built by matching the two netlists on three
independent signals — footprint position (the carrier is the same physical board,
translated by `(-29.07, -0.66)` mm), `(value, named-net-set)`, and value-unique-on-
both-boards — then cross-checking them against each other. **120 of 145 parts
matched with zero conflicts between the three signals, and the mapping is
injective.**

The 25 unmatched parts are genuinely interchangeable and have no meaningful 1:1
identity: same-rail decoupling/bulk caps (`C1`–`C3`, `C5`, `C7`, `C8`, `C10`,
`C11`, `C13`, `C15`, `C20`, `C21`, `C30`, `C37`, `C52`, `C55`, `C14`), the
symmetric USB CC pull-downs `R13`/`R14`, `R6`, the GND/12V test points `TP2`,
`TP3`, `TP8`, `TP12`, and the reference-only `FSR1` symbol.

**Two of those were resolved afterwards by proximity**, because they sit at named
roles rather than being free-floating bulk: `C37` (AMS1117 *input*) → **`C306`**,
5.53 mm from `U303` versus `C302`'s 8.38 mm, so `C52` → **`C302`**. The same check
independently corroborates the AP7361C pair already matched by net signature —
`C44` → `C303` and `C50` → `C307` sit 7.18 mm and 6.80 mm from `U302`, while the
two 12 V caps are 21+ mm away.

Conventions on `dual-panel`: **carrier = 2xx, brain = 3xx.**

## Carrier (2xx)

| panel-pcb | dual-panel | part |
|---|---|---|
| `C22` | **`C203`** | 100nF X7R 50V |
| `C23` | **`C204`** | 100nF X7R 50V |
| `C24` | **`C205`** | 100nF X7R 50V |
| `C25` | **`C206`** | 100nF X7R 50V |
| `C26` | **`C207`** | 100nF X7R 50V |
| `C27` | **`C208`** | 100nF X7R 50V |
| `C28` | **`C209`** | 100nF X7R 50V |
| `C29` | **`C210`** | 100nF X7R 50V |
| `C31` | **`C212`** | 100nF X7R 50V |
| `C32` | **`C213`** | 100nF X7R 50V |
| `C33` | **`C214`** | 100nF X7R 50V |
| `C34` | **`C215`** | 100nF X7R 50V |
| `C35` | **`C216`** | 100nF X7R 50V |
| `C36` | **`C217`** | 100nF X7R 50V |
| `C39` | **`C218`** | 100nF X7R 50V |
| `C40` | **`C219`** | 100nF X7R 50V |
| `C41` | **`C220`** | 100nF X7R 50V |
| `C42` | **`C221`** | 100nF X7R 50V |
| `C43` | **`C222`** | 100nF X7R 50V |
| `C45` | **`C223`** | 100nF X7R 50V |
| `C46` | **`C224`** | 100nF X7R 50V |
| `C47` | **`C225`** | 100nF X7R 50V |
| `C48` | **`C226`** | 100nF X7R 50V |
| `C49` | **`C227`** | 100nF X7R 50V |
| `C51` | **`C201`** | 470uF elec 25V |
| `C57` | **`C202`** | 100nF X7R 50V |
| `D1` | **`D202`** | DEBUG |
| `D2` | **`D203`** | WS2815 |
| `D3` | **`D204`** | WS2815 |
| `D4` | **`D205`** | WS2815 |
| `D5` | **`D206`** | WS2815 |
| `D6` | **`D207`** | WS2815 |
| `D7` | **`D208`** | WS2815 |
| `D8` | **`D209`** | WS2815 |
| `D9` | **`D210`** | WS2815 |
| `D10` | **`D211`** | WS2815 |
| `D11` | **`D212`** | WS2815 |
| `D13` | **`D213`** | WS2815 |
| `D14` | **`D214`** | WS2815 |
| `D15` | **`D215`** | WS2815 |
| `D16` | **`D216`** | WS2815 |
| `D17` | **`D217`** | WS2815 |
| `D18` | **`D218`** | WS2815 |
| `D19` | **`D219`** | WS2815 |
| `D20` | **`D220`** | WS2815 |
| `D21` | **`D221`** | WS2815 |
| `D22` | **`D222`** | WS2815 |
| `D24` | **`D223`** | WS2815 |
| `D25` | **`D224`** | WS2815 |
| `D26` | **`D225`** | WS2815 |
| `D27` | **`D226`** | WS2815 |
| `D28` | **`D227`** | WS2815 |
| `D30` | **`D201`** | SMAJ5.0A |
| `J2` | **`J209`** | SWD |
| `J3` | **`J206`** | FSR North |
| `J4` | **`J203`** | FSR East |
| `J5` | **`J205`** | 12V_IN |
| `J6` | **`J202`** | FSR South |
| `J7` | **`J201`** | FSR West |
| `J8` | **`J204`** | RS-485 IN |
| `J9` | **`J214`** | INT OUT |
| `J10` | **`J207`** | RS-485 OUT |
| `J11` | **`J208`** | 12V_OUT |
| `R2` | **`R201`** | 120R |
| `R15` | **`R204`** | 1k 1% |
| `R17` | **`R203`** | 100R |
| `R20` | **`R202`** | 1M |
| `SW1` | **`SW201`** | PANEL_ID |
| `SW3` | **`SW202`** | RS-485 Termination |
| `TP9` | **`TP214`** | LED Data 5.0V |

## Brain (3xx)

| panel-pcb | dual-panel | part |
|---|---|---|
| `C4` | **`C318`** | 2.2uF X5R 16V |
| `C6` | **`C319`** | 1uF X5R 16V |
| `C9` | **`C320`** | 1uF X5R 16V |
| `C12` | **`C311`** | 15pF C0G 50V |
| `C16` | **`C326`** | 10nF C0G 50V |
| `C17` | **`C330`** | 10nF C0G 50V |
| `C18` | **`C324`** | 10nF C0G 50V |
| `C19` | **`C329`** | 10nF C0G 50V |
| `C38` | **`C308`** | 22uF tant 16V |
| `C44` | **`C303`** | 10uF X5R 50V |
| `C50` | **`C307`** | 10uF X5R 50V |
| `C53` | **`C327`** | 100nF X7R 50V |
| `C54` | **`C304`** | 1uF X5R 16V |
| `C56` | **`C305`** | 1uF X5R 16V |
| `D12` | **`D301`** | PMEG3015EH |
| `D23` | **`D302`** | PMEG3015EH |
| `D29` | **`D303`** | PMEG3015EH |
| `J1` | **`J305`** | USB_C_Receptacle_USB2.0_16P |
| `R1` | **`R307`** | 10k |
| `R3` | **`R305`** | 27R 1% |
| `R4` | **`R306`** | 27R 1% |
| `R5` | **`R308`** | 200R 1% |
| `R7` | **`R309`** | 1k 1% |
| `R8` | **`R315`** | 10k 1% |
| `R9` | **`R311`** | 10k 1% |
| `R10` | **`R316`** | 10k 1% |
| `R11` | **`R312`** | 10k 1% |
| `R12` | **`R310`** | 10k |
| `R16` | **`R301`** | 330R |
| `R18` | **`R313`** | 100k |
| `R19` | **`R314`** | 33k |
| `SW2` | **`SW301`** | BOOTSEL |
| `TP1` | **`TP303`** | RUN |
| `TP4` | **`TP305`** | UART RX |
| `TP5` | **`TP304`** | UART TX |
| `TP6` | **`TP306`** | RS-485 DE |
| `TP7` | **`TP307`** | LED Data 3.3V |
| `TP10` | **`TP302`** | +5VDC |
| `TP11` | **`TP301`** | +3.3VDC |
| `TP13` | **`TP311`** | RS485+ |
| `TP14` | **`TP312`** | RS485- |
| `U1` | **`U306`** | RP2040 |
| `U2` | **`U308`** | THVD1429 |
| `U3` | **`U307`** | W25Q32JV |
| `U4` | **`U301`** | SN74AHCT1G125 |
| `U5` | **`U303`** | AMS1117-5.0 |
| `U6` | **`U302`** | AP7361C-33ER-13 |
| `U7` | **`U305`** | USBLC6-2SC6 |
| `U8` | **`U304`** | LM66200 |
| `X1` | **`X301`** | 12MHz ABM8-272-T3 |

## Things the table alone won't tell you

- **The board-to-board interface did not exist on `panel-pcb`.** Carrier
  `J210`–`J213` and brain `J301`–`J304` are new, and so are the second set of
  mounting holes.
- **USB-C moved boards.** `J1` → `J305`, carrier → **brain** (2026-07-28). Older
  text saying "USB-C stays on the carrier" describes a plan that was reversed.
- **The FSR dividers and their caps moved to the brain**, so `FSR_North/East/
  South/West` became interface-crossing nets rather than board-local ones.
- **The RP2040 GPIO map changed** — see `docs/DUAL_PANEL.md`. A refdes translation
  does *not* translate pin numbers.

