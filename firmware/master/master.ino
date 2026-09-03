// Master MCU firmware — Teensy 4.0, as-built master-pcb rev 1.
//
// Ported off the 2-panel breadboard prototype 2026-09-03: pins, panel count and
// UART all come from the netlist now (docs/MASTER_PCB.md "Teensy 4.0 pin map").
// The old prototype map is NOT a subset of this one — pins 3 and 4 carried the
// two INT lines there and carry the player-ID DIP here, so a partial port would
// have read the DIP as panel presses.
//
// Runs the pad: INT edge capture, addressed LED frames, FSR telemetry polling.
// Also carries the bench command set, because master bring-up happens with zero
// panels flashed and every board-local check has to work standalone.
//
// Protocol: docs/RS485_PROTOCOL.md. Bring-up procedure: docs/MASTER_BRINGUP.md.
// Flash: arduino-cli compile --fqbn teensy:avr:teensy40 firmware/master
//        arduino-cli upload  --fqbn teensy:avr:teensy40 firmware/master

// ── As-built pin map (netlist-verified against master-pcb.kicad_pcb) ─────────
constexpr uint8_t NUM_PANELS = 9;
constexpr uint8_t PANEL_IDS[NUM_PANELS] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

// Panel ID -> physical position -> Teensy pin -> master connector. The position
// string is exactly what is silkscreened beside the header and printed on the
// cable label, so anything reported by name needs no lookup table at the bench.
constexpr uint8_t INT_PINS[NUM_PANELS]  = {23, 22, 21, 20, 19, 18, 17, 16, 15};
const char *const PANEL_NAME[NUM_PANELS] = {"UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"};
const char *const INT_CONN[NUM_PANELS]   = {"J11","J10","J9","J8","J7","J6","J5","J4","J3"};

// RS-485 is Serial2 (RX 7 / TX 8) with DE on 6. Serial1 (pins 0/1) is
// deliberately left free as the last spare hardware UART.
#define RS485_SERIAL Serial2
constexpr uint8_t RS485_DE_PIN = 6;   // Serial2.transmitterEnable() drives this

// Player/pad ID DIP (SW1). Closes to GND against internal pull-ups, so a CLOSED
// switch reads 0 — meaning player 0 (P1) is all three switches ON, and a
// factory-fresh all-OFF switch reads 7, not 0. Bit order runs backwards against
// ascending pin number, exactly as on the panel's ID DIP.
constexpr uint8_t DIP_ID2_PIN = 3;
constexpr uint8_t DIP_ID1_PIN = 4;
constexpr uint8_t DIP_ID0_PIN = 5;

// Underglow WS2811 data out -> U3 (SN74AHCT1G125, 5V) -> R5 330R -> J2 pin 1.
// R4 (10k pull-down) holds U3's input LOW before firmware drives the pin, so
// the strip sees no garbage during boot. 44 groups of 3 LEDs; the WS2811 drives
// 3 LEDs per chip, so one group = one pixel of data.
constexpr uint8_t UNDERGLOW_PIN = 11;
constexpr int     UNDERGLOW_GROUPS = 44;

constexpr uint8_t LED_PIN = 13;       // Teensy onboard LED; not routed on the board

struct Color { uint8_t r, g, b; };
constexpr Color PANEL_PRESS_COLOR[NUM_PANELS] = {
  {200,   0,   0},  // UL
  {200, 100,   0},  // U
  {200, 200,   0},  // UR
  {  0, 200,   0},  // L
  {  0, 200, 200},  // C
  {  0,   0, 200},  // R
  {120,   0, 200},  // DL
  {200,   0, 120},  // D
  {160, 160, 160},  // DR
};

constexpr uint32_t RS485_BAUD = 1000000;
constexpr uint8_t  PKT_SYNC = 0x55;
constexpr uint8_t  PKT_MAX_PAYLOAD = 80;
constexpr uint8_t  BROADCAST = 0xFF;
constexpr int      NUM_LEDS = 25;
constexpr uint32_t FRAME_INTERVAL_MS = 16;    // ~60Hz per-panel addressed frames
constexpr uint32_t POLL_INTERVAL_MS = 5;      // 200Hz FSR telemetry poll, independent of LED rate
constexpr uint32_t STREAM_INTERVAL_MS = 33;   // ~30Hz print rate when streaming ('t' to toggle)

int panelIndex(uint8_t addr) {
  for (int i = 0; i < NUM_PANELS; i++)
    if (PANEL_IDS[i] == addr) return i;
  return -1;
}

// ── INT edge capture (ISR -> loop ring buffer) ───────────────────────────────
// Parallel volatile arrays rather than an array-of-structs: the implicit
// copy constructor won't bind to a volatile-qualified struct lvalue, but
// plain volatile scalar reads/writes work fine.
constexpr size_t RING_SIZE = 64;
volatile uint32_t ring_t_us[RING_SIZE];
volatile uint8_t  ring_panel[RING_SIZE];
volatile uint8_t  ring_pressed[RING_SIZE];
volatile size_t   ring_head = 0;
volatile size_t   ring_tail = 0;
volatile uint32_t dropped_edges = 0;

inline void pushEdge(uint8_t panel, uint8_t pin) {
  uint8_t pressed = (digitalReadFast(pin) == LOW) ? 1 : 0;  // active-low
  size_t next = (ring_head + 1) % RING_SIZE;
  if (next == ring_tail) {
    dropped_edges++;
    return;
  }
  ring_t_us[ring_head] = micros();
  ring_panel[ring_head] = panel;
  ring_pressed[ring_head] = pressed;
  ring_head = next;
}
// attachInterrupt needs a plain function pointer per pin — no capture, so one
// trampoline per panel rather than a single parameterized handler.
#define INT_TRAMPOLINE(n) void intLineIsr##n() { pushEdge(n, INT_PINS[n]); }
INT_TRAMPOLINE(0) INT_TRAMPOLINE(1) INT_TRAMPOLINE(2)
INT_TRAMPOLINE(3) INT_TRAMPOLINE(4) INT_TRAMPOLINE(5)
INT_TRAMPOLINE(6) INT_TRAMPOLINE(7) INT_TRAMPOLINE(8)
void (*const INT_ISR[NUM_PANELS])() = {
  intLineIsr0, intLineIsr1, intLineIsr2, intLineIsr3, intLineIsr4,
  intLineIsr5, intLineIsr6, intLineIsr7, intLineIsr8,
};

// ── RS-485 framing ───────────────────────────────────────────────────────────
uint8_t crc8Update(uint8_t crc, uint8_t b) {
  crc ^= b;
  for (int i = 0; i < 8; i++)
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  return crc;
}

void sendPacket(uint8_t cmd, uint8_t addr, const uint8_t *payload, uint8_t len) {
  uint8_t crc = 0;
  crc = crc8Update(crc, cmd);
  crc = crc8Update(crc, addr);
  crc = crc8Update(crc, len);
  for (int i = 0; i < len; i++) crc = crc8Update(crc, payload[i]);

  RS485_SERIAL.write(PKT_SYNC);
  RS485_SERIAL.write(cmd);
  RS485_SERIAL.write(addr);
  RS485_SERIAL.write(len);
  if (len) RS485_SERIAL.write(payload, len);
  RS485_SERIAL.write(crc);
  // transmitterEnable handles DE timing; no flush needed before queuing more
}

// ── RX parser (panel replies) ────────────────────────────────────────────────
uint16_t panel_fsr[NUM_PANELS][4] = {};
uint8_t  panel_pressed_mask[NUM_PANELS] = {};
bool     stream_telemetry = false;
uint32_t stat_replies = 0, stat_crc_errs = 0, stat_polls_sent = 0, stat_frames_sent = 0;
// Per-panel poll success, reset each heartbeat window. A globally elevated
// CRC error rate can mean many things (noise, bad termination, a flaky
// cable) — but one specific address having a much worse reply rate than the
// others is the signature of a duplicate panel ID (two boards both answering
// polls addressed to that ID collide on the bus). Tracking per-address, not
// just in aggregate, is what makes that distinguishable.
uint32_t poll_sent_window[NUM_PANELS] = {};
uint32_t poll_ok_window[NUM_PANELS] = {};
// Lifetime per-panel reply count. The windowed counters reset every heartbeat,
// so they cannot answer "has this panel EVER answered" — which is what decides
// whether a panel is worth printing a line for.
uint32_t poll_ok_total[NUM_PANELS] = {};

extern volatile int ident_ack_id;   // defined with the bench checks below

void parseRx() {
  static enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
  static uint8_t cmd, addr, len, idx;
  static uint8_t pay[PKT_MAX_PAYLOAD];

  while (RS485_SERIAL.available()) {
    uint8_t c = (uint8_t)RS485_SERIAL.read();
    switch (st) {
      case W_SYNC: if (c == PKT_SYNC) st = W_CMD; break;
      case W_CMD:  cmd = c; st = W_ADDR; break;
      case W_ADDR: addr = c; st = W_LEN; break;
      case W_LEN:
        len = c; idx = 0;
        if (len > PKT_MAX_PAYLOAD) { st = W_SYNC; break; }
        st = (len > 0) ? W_PAY : W_CRC;
        break;
      case W_PAY:
        pay[idx++] = c;
        if (idx == len) st = W_CRC;
        break;
      case W_CRC: {
        uint8_t crc = 0;
        crc = crc8Update(crc, cmd);
        crc = crc8Update(crc, addr);
        crc = crc8Update(crc, len);
        for (int i = 0; i < len; i++) crc = crc8Update(crc, pay[i]);
        int pidx = panelIndex(addr);
        if (crc != c) {
          stat_crc_errs++;
        } else if (cmd == 'f' && len == 9 && pidx >= 0) {
          for (int i = 0; i < 4; i++)
            panel_fsr[pidx][i] = (uint16_t)pay[2 * i] | ((uint16_t)pay[2 * i + 1] << 8);
          panel_pressed_mask[pidx] = pay[8];
          stat_replies++;
          poll_ok_window[pidx]++;
          poll_ok_total[pidx]++;
        } else if (cmd == 'i' && len == 0) {
          ident_ack_id = addr;
        } else if (cmd == 'c' && len == 5) {
          uint16_t press = (uint16_t)pay[1] | ((uint16_t)pay[2] << 8);
          uint16_t rel   = (uint16_t)pay[3] | ((uint16_t)pay[4] << 8);
          Serial.print("# panel ");
          Serial.print(addr);
          Serial.print(" ack thresholds: press=");
          Serial.print(press);
          Serial.print(" release=");
          Serial.println(rel);
        }
        st = W_SYNC;
        break;
      }
    }
  }
}

// ── LED patterns ─────────────────────────────────────────────────────────────
// Classic 0-255 color wheel -> RGB
void wheel(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b) {
  pos = 255 - pos;
  if (pos < 85)       { r = 255 - pos * 3; g = 0;            b = pos * 3; }
  else if (pos < 170) { pos -= 85; r = 0;  g = pos * 3;      b = 255 - pos * 3; }
  else                { pos -= 170; r = pos * 3; g = 255 - pos * 3; b = 0; }
}

void buildIdleFrame(uint8_t *frame, uint32_t t_ms) {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t r, g, b;
    wheel((uint8_t)((i * 256 / NUM_LEDS + t_ms / 8) & 0xFF), r, g, b);
    frame[3 * i]     = r >> 3;  // dim to ~1/8 brightness — breadboard-friendly current
    frame[3 * i + 1] = g >> 3;
    frame[3 * i + 2] = b >> 3;
  }
}

void buildSolidFrame(uint8_t *frame, Color c) {
  for (int i = 0; i < NUM_LEDS; i++) {
    frame[3 * i]     = c.r;
    frame[3 * i + 1] = c.g;
    frame[3 * i + 2] = c.b;
  }
}

// ── Board-local bench checks ─────────────────────────────────────────────────
// Everything in this section works with zero panels connected, which is the
// state the master is in when it is first powered.

bool bus_traffic = true;   // 'x' gates the LED-frame + poll loop

uint8_t readPlayerId() {
  // Closed switch = 0 (closes to GND against the internal pull-up), and the bit
  // order runs backwards against pin number: pin 3 is bit 2, pin 5 is bit 0.
  return (uint8_t)((digitalReadFast(DIP_ID2_PIN) << 2) |
                   (digitalReadFast(DIP_ID1_PIN) << 1) |
                    digitalReadFast(DIP_ID0_PIN));
}

void reportIntLines(const char *when) {
  Serial.print("INT lines ");
  Serial.print(when);
  Serial.println(":");
  for (int i = 0; i < NUM_PANELS; i++) {
    bool low = (digitalReadFast(INT_PINS[i]) == LOW);
    Serial.print("  ");
    Serial.print(PANEL_NAME[i]);
    Serial.print("\t panel ");
    Serial.print(PANEL_IDS[i]);
    Serial.print("\t pin ");
    Serial.print(INT_PINS[i]);
    Serial.print("\t ");
    Serial.print(INT_CONN[i]);
    Serial.println(low ? "\t LOW  <- pressed, stuck FSR, or shorted wire"
                       : "\t HIGH (idle)");
  }
}

// docs/MASTER_PCB.md flags this as "worth confirming at bring-up, not verified
// here": all nine INT pins are AD_B1_xx pads and should therefore sit on a
// single i.MX RT GPIO port, which would let a future fast path sample every
// panel in one register read. Confirm it from the core's own pin tables rather
// than from the datasheet — and note the bit positions, because they are NOT
// contiguous: one read yes, one shift no.
void reportIntPortLayout() {
  Serial.println("INT pin -> GPIO port register / bit:");
  volatile uint32_t *first = nullptr;
  bool same = true;
  const volatile uint32_t *regs[NUM_PANELS];
  const uint32_t masks[NUM_PANELS] = {
    CORE_PIN23_BITMASK, CORE_PIN22_BITMASK, CORE_PIN21_BITMASK,
    CORE_PIN20_BITMASK, CORE_PIN19_BITMASK, CORE_PIN18_BITMASK,
    CORE_PIN17_BITMASK, CORE_PIN16_BITMASK, CORE_PIN15_BITMASK,
  };
  regs[0] = &CORE_PIN23_PINREG; regs[1] = &CORE_PIN22_PINREG;
  regs[2] = &CORE_PIN21_PINREG; regs[3] = &CORE_PIN20_PINREG;
  regs[4] = &CORE_PIN19_PINREG; regs[5] = &CORE_PIN18_PINREG;
  regs[6] = &CORE_PIN17_PINREG; regs[7] = &CORE_PIN16_PINREG;
  regs[8] = &CORE_PIN15_PINREG;

  for (int i = 0; i < NUM_PANELS; i++) {
    if (i == 0) first = (volatile uint32_t *)regs[0];
    else if (regs[i] != first) same = false;
    int bit = 0;
    while (bit < 32 && !(masks[i] & (1UL << bit))) bit++;
    Serial.print("  ");
    Serial.print(PANEL_NAME[i]);
    Serial.print("\t pin ");
    Serial.print(INT_PINS[i]);
    Serial.print("\t reg 0x");
    Serial.print((uint32_t)(uintptr_t)regs[i], HEX);
    Serial.print("\t bit ");
    Serial.println(bit);
  }
  Serial.println(same
    ? "  => CONFIRMED: all nine on one register. A single read samples the pad;\n"
      "     the bits are scattered, so it needs a table, not a shift."
    : "  => NOT all on one register — the single-read fast path is not available.");
}

// WS2811 800kHz bit-bang. Approximate by design: this is a presence check for
// U3 and the cable, not a driver. T1H ~600ns / T0H ~250ns, 1.25us period, and
// the part tolerates +-150ns. Verify against a scope once, then trust it.
void underglowSendByte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    if (b & (1 << i)) {
      digitalWriteFast(UNDERGLOW_PIN, HIGH); delayNanoseconds(600);
      digitalWriteFast(UNDERGLOW_PIN, LOW);  delayNanoseconds(600);
    } else {
      digitalWriteFast(UNDERGLOW_PIN, HIGH); delayNanoseconds(250);
      digitalWriteFast(UNDERGLOW_PIN, LOW);  delayNanoseconds(950);
    }
  }
}

void underglowFill(uint8_t r, uint8_t g, uint8_t b) {
  noInterrupts();
  for (int i = 0; i < UNDERGLOW_GROUPS; i++) {
    underglowSendByte(r); underglowSendByte(g); underglowSendByte(b);
  }
  interrupts();
  delayMicroseconds(300);   // latch
}

void underglowTest() {
  // Underglow is WS2811, not WS2815: three INDEPENDENT constant-current sinks,
  // so colour scales current linearly here — the opposite of the panels. Keep
  // the test dim and remember the strip's 12V comes from the Wago fan-out, not
  // from this board: with the fan-out unpowered this drives data into dead LEDs
  // and proves only that U3 switches.
  Serial.println("# underglow: red, green, blue, dim white, off");
  underglowFill(40, 0, 0);  delay(600);
  underglowFill(0, 40, 0);  delay(600);
  underglowFill(0, 0, 40);  delay(600);
  underglowFill(20, 20, 20); delay(1200);
  underglowFill(0, 0, 0);
  Serial.println("# done. No 12V at the fan-out means no light — check U3's Y pin instead.");
}

// ── Slot <-> panel-ID self-test (docs/RS485_PROTOCOL.md) ─────────────────────
// The master half of 'I'. The panel half is not implemented yet, so until the
// panel firmware is ported this reports "no ack" for every ID — which is a
// correct result, not a failure of this code.
volatile int  ident_ack_id = -1;

void identifySelfTest() {
  constexpr uint8_t PULSE_MS = 2;
  constexpr uint32_t ACK_TIMEOUT_MS = 20;
  constexpr uint32_t EDGE_WINDOW_MS = 50;

  // Step 1: idle pre-check. A line already LOW cannot show a transition, so
  // mis-attributing a later pulse is the real risk — stop instead.
  bool blocked = false;
  for (int i = 0; i < NUM_PANELS; i++) {
    if (digitalReadFast(INT_PINS[i]) == LOW) {
      Serial.print("# pre-check FAIL: ");
      Serial.print(PANEL_NAME[i]);
      Serial.print(" (pin ");
      Serial.print(INT_PINS[i]);
      Serial.println(") is already LOW — stood-on panel, stuck FSR, or shorted wire");
      blocked = true;
    }
  }
  if (blocked) { Serial.println("# aborting self-test"); return; }

  bool was = bus_traffic;
  bus_traffic = false;   // no LED frames or polls competing for the bus
  Serial.println("# slot <-> panel-ID self-test");

  for (int id = 0; id < NUM_PANELS; id++) {
    ident_ack_id = -1;
    uint8_t payload[1] = {PULSE_MS};
    sendPacket('I', (uint8_t)id, payload, 1);

    uint32_t t0 = millis();
    while (ident_ack_id != id && (millis() - t0) < ACK_TIMEOUT_MS) parseRx();
    bool acked = (ident_ack_id == id);

    // Watch all nine, not just the expected one — that is the whole point.
    int      fired[NUM_PANELS]; int nfired = 0;
    uint32_t low_us[NUM_PANELS] = {};
    bool     is_low[NUM_PANELS] = {};
    uint32_t start_us[NUM_PANELS] = {};
    uint32_t w0 = millis();
    while ((millis() - w0) < EDGE_WINDOW_MS) {
      for (int i = 0; i < NUM_PANELS; i++) {
        bool low = (digitalReadFast(INT_PINS[i]) == LOW);
        if (low && !is_low[i]) { is_low[i] = true; start_us[i] = micros(); }
        else if (!low && is_low[i]) {
          is_low[i] = false;
          low_us[i] = micros() - start_us[i];
          if (nfired < NUM_PANELS) fired[nfired++] = i;
        }
      }
    }

    Serial.print("  ID ");
    Serial.print(id);
    Serial.print(": ack=");
    Serial.print(acked ? "yes" : "NO ");
    Serial.print("  slots fired=");
    Serial.print(nfired);
    if (nfired == 1) {
      Serial.print(" -> ");
      Serial.print(PANEL_NAME[fired[0]]);
      Serial.print(" (");
      Serial.print(INT_CONN[fired[0]]);
      Serial.print(", ");
      Serial.print(low_us[fired[0]]);
      Serial.print("us low)");
      if (fired[0] != id) Serial.print("   !! MISMATCH — cable in the wrong header");
    } else if (nfired > 1) {
      Serial.print(" -> ");
      for (int k = 0; k < nfired; k++) { Serial.print(PANEL_NAME[fired[k]]); Serial.print(' '); }
      Serial.print("  !! two panels share this DIP ID");
    } else if (acked) {
      Serial.print("   !! acked but no edge — INT wire not landed, or panel GPIO dead");
    }
    Serial.println();
  }
  bus_traffic = was;
  Serial.println("# self-test done");
}

// ── USB serial commands ──────────────────────────────────────────────────────
// Newline-terminated. The board-local ones (?, n, r, D, u, x) need no panels.
void printHelp() {
  Serial.println(F(
    "commands:\n"
    "  ?                          this help\n"
    "  n                          INT line states, by panel position\n"
    "  r                          INT GPIO port/bit layout (single-read check)\n"
    "  D                          read the player-ID DIP (SW1)\n"
    "  u                          underglow test pattern\n"
    "  x                          pause/resume LED frames + FSR polling\n"
    "  I                          slot <-> panel-ID self-test (needs panels)\n"
    "  t                          toggle telemetry stream\n"
    "  S <panel> <press> <rel>    set FSR thresholds on one panel"));
}

void handleCommand(const char *s) {
  if (strcmp(s, "?") == 0) {
    printHelp();
  } else if (strcmp(s, "n") == 0) {
    reportIntLines("now");
  } else if (strcmp(s, "r") == 0) {
    reportIntPortLayout();
  } else if (strcmp(s, "D") == 0 || strcmp(s, "dip") == 0) {
    uint8_t id = readPlayerId();
    Serial.print("# player ID = ");
    Serial.print(id);
    Serial.print("  (P");
    Serial.print(id + 1);
    Serial.print(id < 4 ? ")" : ", reserved code)");
    Serial.println("   closed switch = 0, so P1 is all three ON");
  } else if (strcmp(s, "u") == 0) {
    underglowTest();
  } else if (strcmp(s, "x") == 0) {
    bus_traffic = !bus_traffic;
    Serial.print("# bus traffic ");
    Serial.println(bus_traffic ? "RESUMED" : "PAUSED");
  } else if (strcmp(s, "I") == 0) {
    identifySelfTest();
  } else if (strcmp(s, "t") == 0 || strcmp(s, "T") == 0) {
    stream_telemetry = !stream_telemetry;
    Serial.print("# telemetry stream ");
    Serial.println(stream_telemetry ? "ON" : "OFF");
  } else if (s[0] == 'S' || s[0] == 's') {
    int panel, press, rel;
    if (sscanf(s + 1, "%d %d %d", &panel, &press, &rel) == 3 &&
        panel >= 0 && panel < NUM_PANELS &&
        press > 0 && press <= 4095 && rel > 0 && rel < press) {
      uint8_t payload[5] = {
        0xFF,  // all channels
        (uint8_t)(press & 0xFF), (uint8_t)(press >> 8),
        (uint8_t)(rel & 0xFF),   (uint8_t)(rel >> 8),
      };
      sendPacket('C', PANEL_IDS[panel], payload, sizeof(payload));
      // panel echoes 'c' ack, printed by parseRx
    } else {
      Serial.println("# usage: S <panel 0-8> <press 1-4095> <release, below press>");
    }
  }
}

void pollSerialCommands() {
  static char buf[32];
  static uint8_t len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        buf[len] = '\0';
        handleCommand(buf);
        len = 0;
      }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}

// Called once per heartbeat window. A globally elevated CRC error rate can
// mean many things; a single address with a much worse reply rate than the
// rest is specifically the signature of two boards sharing that panel ID
// (their replies collide on the bus). Resets the window either way.
void checkPanelHealth() {
  constexpr uint32_t MIN_SAMPLE = 20;   // don't judge on too few polls
  constexpr uint32_t BAD_PCT = 50;
  constexpr uint32_t GOOD_PCT = 90;
  int worst = -1, best = -1;
  uint32_t worst_rate = 101, best_rate = 0;

  for (int i = 0; i < NUM_PANELS; i++) {
    if (poll_sent_window[i] < MIN_SAMPLE) continue;
    uint32_t rate = (poll_ok_window[i] * 100) / poll_sent_window[i];
    if (rate < worst_rate) { worst_rate = rate; worst = i; }
    if (rate > best_rate)  { best_rate = rate; best = i; }
  }

  if (worst >= 0 && best >= 0 && worst != best &&
      worst_rate < BAD_PCT && best_rate > GOOD_PCT) {
    Serial.print("# !! WARNING: panel ");
    Serial.print(PANEL_IDS[worst]);
    Serial.print(" poll reply rate is only ");
    Serial.print(worst_rate);
    Serial.print("% while panel ");
    Serial.print(PANEL_IDS[best]);
    Serial.print(" is ");
    Serial.print(best_rate);
    Serial.println("% -- check for a duplicate panel ID (two boards on the "
                    "same address collide when both reply on the bus)");
  }

  for (int i = 0; i < NUM_PANELS; i++) {
    poll_sent_window[i] = 0;
    poll_ok_window[i] = 0;
  }
}

// ── Setup / loop ─────────────────────────────────────────────────────────────
void setup() {
  // RN1 (10k x9) already pulls every INT line to +3.3VDC on the board. The
  // internal pull-up is belt-and-braces and costs nothing; it also keeps the
  // line defined if RN1 is ever unpopulated on a bare-board build.
  for (int i = 0; i < NUM_PANELS; i++)
    pinMode(INT_PINS[i], INPUT_PULLUP);
  pinMode(DIP_ID0_PIN, INPUT_PULLUP);   // no board resistors on the DIP nets
  pinMode(DIP_ID1_PIN, INPUT_PULLUP);
  pinMode(DIP_ID2_PIN, INPUT_PULLUP);
  pinMode(UNDERGLOW_PIN, OUTPUT);
  digitalWriteFast(UNDERGLOW_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) {}

  RS485_SERIAL.begin(RS485_BAUD);
  RS485_SERIAL.transmitterEnable(RS485_DE_PIN);  // hardware DE: asserted during TX only

  Serial.println("Master firmware — multi-panel INT monitor + RS-485 bus");
  reportIntLines("at boot");

  for (int i = 0; i < NUM_PANELS; i++)
    attachInterrupt(digitalPinToInterrupt(INT_PINS[i]), INT_ISR[i], CHANGE);

  Serial.print("player ID (SW1): ");
  Serial.print(readPlayerId());
  Serial.println("   [P1=0 requires all three switches ON; all-OFF reads 7]");
  Serial.println("'?' for the bench command set.");
}

void loop() {
  static uint32_t press_start_us[NUM_PANELS] = {};
  static bool panel_pressed[NUM_PANELS] = {};
  static uint32_t next_heartbeat_ms = 5000;
  static uint32_t next_frame_ms = 0;
  static uint32_t next_poll_ms = 0;
  static uint32_t next_stream_ms = 0;

  parseRx();
  pollSerialCommands();

  // Drain INT edge ring buffer
  while (ring_tail != ring_head) {
    uint32_t t_us = ring_t_us[ring_tail];
    uint8_t  panel = ring_panel[ring_tail];
    bool     pressed = ring_pressed[ring_tail] != 0;
    ring_tail = (ring_tail + 1) % RING_SIZE;

    if (pressed == panel_pressed[panel]) continue;
    panel_pressed[panel] = pressed;

    if (pressed) {
      press_start_us[panel] = t_us;
      Serial.print("PRESS   panel ");
      Serial.print(panel);
      Serial.print(" @ ");
      Serial.print(t_us);
      Serial.println(" us");
    } else {
      Serial.print("RELEASE panel ");
      Serial.print(panel);
      Serial.print(" @ ");
      Serial.print(t_us);
      Serial.print(" us  (held ");
      Serial.print((t_us - press_start_us[panel]) / 1000.0f, 3);
      Serial.println(" ms)");
    }
  }

  bool any_pressed = false;
  for (int i = 0; i < NUM_PANELS; i++) any_pressed |= panel_pressed[i];
  digitalWriteFast(LED_PIN, any_pressed ? HIGH : LOW);

  // Per-panel addressed LED frames at 60Hz: solid color if that panel is
  // pressed, dim rainbow chase otherwise.
  uint32_t now_ms = millis();
  if (bus_traffic && now_ms >= next_frame_ms) {
    next_frame_ms = now_ms + FRAME_INTERVAL_MS;
    for (int i = 0; i < NUM_PANELS; i++) {
      uint8_t frame[NUM_LEDS * 3];
      if (panel_pressed[i]) buildSolidFrame(frame, PANEL_PRESS_COLOR[i]);
      else                  buildIdleFrame(frame, now_ms);
      sendPacket('L', PANEL_IDS[i], frame, sizeof(frame));
      stat_frames_sent++;
    }
  }

  // FSR poll on its own clock — telemetry rate is independent of the LED rate.
  // One panel per tick, round-robin: polling all panels back-to-back left no
  // gap for a reply to finish before the next poll went out, risking a real
  // half-duplex bus collision between one panel's reply and the master's next
  // transmission. 5ms >> the ~150us a full poll-reply round trip takes, so
  // round-robining leaves each panel an unambiguous, collision-free window.
  static uint8_t poll_panel_idx = 0;
  if (bus_traffic && now_ms >= next_poll_ms) {
    next_poll_ms = now_ms + POLL_INTERVAL_MS;
    sendPacket('F', PANEL_IDS[poll_panel_idx], nullptr, 0);
    stat_polls_sent++;
    poll_sent_window[poll_panel_idx]++;
    poll_panel_idx = (poll_panel_idx + 1) % NUM_PANELS;
  }

  if (stream_telemetry && now_ms >= next_stream_ms) {
    next_stream_ms = now_ms + STREAM_INTERVAL_MS;
    for (int i = 0; i < NUM_PANELS; i++) {
      Serial.print("T ");
      Serial.print(PANEL_IDS[i]);
      Serial.print(' ');
      for (int c = 0; c < 4; c++) {
        Serial.print(panel_fsr[i][c]);
        Serial.print(' ');
      }
      Serial.println(panel_pressed_mask[i]);
    }
  }

  if (now_ms >= next_heartbeat_ms) {
    next_heartbeat_ms += 5000;
    checkPanelHealth();
    // Nine panels would make the old single-line heartbeat unreadable, and with
    // no panels attached it was nine lines of zeros. Report only what replied.
    Serial.print("[heartbeat] ");
    int seen = 0;
    for (int i = 0; i < NUM_PANELS; i++) if (poll_ok_total[i]) seen++;
    if (seen == 0) {
      Serial.print("no panel replies yet  ");
    } else {
      Serial.println();
      for (int i = 0; i < NUM_PANELS; i++) {
        if (!poll_ok_total[i]) continue;
        Serial.print("  ");
        Serial.print(PANEL_NAME[i]);
        Serial.print("\t id ");
        Serial.print(PANEL_IDS[i]);
        Serial.print("  INT=");
        Serial.print(panel_pressed[i] ? "LOW(pressed)" : "HIGH(idle)");
        Serial.print("  FSR=");
        for (int c = 0; c < 4; c++) {
          Serial.print(panel_fsr[i][c]);
          Serial.print(c < 3 ? "," : "");
        }
        Serial.print("  mask=");
        Serial.println(panel_pressed_mask[i], BIN);
      }
      Serial.print("  ");
    }
    Serial.print("| bus: ");
    Serial.print(stat_replies);
    Serial.print("/");
    Serial.print(stat_polls_sent);
    Serial.print(" poll replies, ");
    Serial.print(stat_frames_sent);
    Serial.print(" frames sent, ");
    Serial.print(stat_crc_errs);
    Serial.print(" crc errs");
    if (dropped_edges) {
      Serial.print(", dropped edges: ");
      Serial.print(dropped_edges);
    }
    Serial.println();
  }
}
