// Master MCU firmware — Teensy 4.0
//
// Multi-panel prototype: INT line monitoring + RS-485 bus for 2 panels.
//   - Each panel gets its own addressed LED frame at 60Hz: dim rainbow chase
//     when idle, solid per-panel color when THAT panel's INT line is active
//     (demonstrating per-panel addressing: FSR -> INT -> master -> addressed
//     bus frame -> that panel's LEDs only)
//   - Polls both panels for FSR telemetry each cycle
//   - Reports INT edges over USB serial with microsecond timestamps, tagged
//     by panel
//
// Protocol: docs/RS485_PROTOCOL.md. Wiring: docs/archive/PROTOTYPE_WIRING.md.
// Flash: arduino-cli compile --fqbn teensy:avr:teensy40 firmware/master
//        arduino-cli upload  --fqbn teensy:avr:teensy40 firmware/master

constexpr uint8_t NUM_PANELS = 2;
constexpr uint8_t PANEL_IDS[NUM_PANELS] = {0, 1};
// Panel 1's INT line is assumed wired to pin 4 (adjacent to panel 0's pin 3) —
// correct this constant if it's actually wired elsewhere.
constexpr uint8_t INT_PINS[NUM_PANELS] = {3, 4};
constexpr uint8_t RS485_DE_PIN = 2;  // Serial1 manages this via transmitterEnable
constexpr uint8_t LED_PIN = 13;

struct Color { uint8_t r, g, b; };
constexpr Color PANEL_PRESS_COLOR[NUM_PANELS] = {
  {200, 0, 0},   // panel 0 -> red
  {0, 200, 0},   // panel 1 -> green
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
void intLineIsr0() { pushEdge(0, INT_PINS[0]); }
void intLineIsr1() { pushEdge(1, INT_PINS[1]); }

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

  Serial1.write(PKT_SYNC);
  Serial1.write(cmd);
  Serial1.write(addr);
  Serial1.write(len);
  if (len) Serial1.write(payload, len);
  Serial1.write(crc);
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

void parseRx() {
  static enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
  static uint8_t cmd, addr, len, idx;
  static uint8_t pay[PKT_MAX_PAYLOAD];

  while (Serial1.available()) {
    uint8_t c = (uint8_t)Serial1.read();
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

// ── USB serial commands ──────────────────────────────────────────────────────
// Newline-terminated:
//   t                              toggle telemetry stream
//   S <panel> <press> <release>    set thresholds on all channels of one panel
void handleCommand(const char *s) {
  if (strcmp(s, "t") == 0 || strcmp(s, "T") == 0) {
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
      Serial.println("# usage: S <panel 0-1> <press 1-4095> <release, below press>");
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
  for (int i = 0; i < NUM_PANELS; i++)
    pinMode(INT_PINS[i], INPUT_PULLUP);  // external 10k on the breadboard; internal as backup
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) {}

  Serial1.begin(RS485_BAUD);
  Serial1.transmitterEnable(RS485_DE_PIN);  // hardware DE: asserted during TX only

  Serial.println("Master firmware — multi-panel INT monitor + RS-485 bus");
  for (int i = 0; i < NUM_PANELS; i++) {
    Serial.print("INT pin ");
    Serial.print(INT_PINS[i]);
    Serial.print(" (panel ");
    Serial.print(PANEL_IDS[i]);
    Serial.print(") at boot: ");
    Serial.println(digitalReadFast(INT_PINS[i]) == LOW ? "LOW (pressed?!)" : "HIGH (idle)");
  }

  attachInterrupt(digitalPinToInterrupt(INT_PINS[0]), intLineIsr0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(INT_PINS[1]), intLineIsr1, CHANGE);
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
  if (now_ms >= next_frame_ms) {
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
  if (now_ms >= next_poll_ms) {
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
    Serial.print("[heartbeat] ");
    for (int i = 0; i < NUM_PANELS; i++) {
      Serial.print("panel");
      Serial.print(PANEL_IDS[i]);
      Serial.print(": INT=");
      Serial.print(panel_pressed[i] ? "LOW(pressed)" : "HIGH(idle)");
      Serial.print(" FSR=");
      for (int c = 0; c < 4; c++) {
        Serial.print(panel_fsr[i][c]);
        Serial.print(c < 3 ? "," : "");
      }
      Serial.print(" mask=");
      Serial.print(panel_pressed_mask[i], BIN);
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
