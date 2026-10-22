// Master MCU firmware — Teensy 4.0, as-built master-pcb rev 1.
//
// Ported off the 2-panel breadboard prototype 2026-09-03: pins, panel count and
// UART all come from the netlist now (docs/MASTER_PCB.md "Teensy 4.0 pin map").
// The old prototype map is NOT a subset of this one — pins 3 and 4 carried the
// two INT lines there and carry the player-ID DIP here, so a partial port would
// have read the DIP as panel presses.
//
// Runs the pad: INT edge capture, addressed LED frames, FSR telemetry polling,
// the panels' config (the only saved copy, pushed to them with 'W'), and the
// USB vendor interface (commands, events, telemetry stream).
// Also carries the bench command set, because master bring-up happens with zero
// panels flashed and every board-local check has to work standalone.
//
// Protocols: docs/RS485_PROTOCOL.md, docs/USB_PROTOCOL.md. Config:
// docs/PANEL_CONFIG.md. Bring-up procedure: docs/MASTER_BRINGUP.md.
// Build: firmware/master/build.sh (compile) or build.sh upload. It builds
//        against a patched Teensy core for the USB interfaces and adds
//        firmware/shared/ to the include path (docs/MASTER_BRINGUP.md).

#if !defined(USB_KRAKENPAD)
#error "Build with firmware/master/build.sh: it selects the KrakenPad USB type and adds firmware/shared/ to the include path."
#endif

#include <EEPROM.h>
#include "panel_config.h"   // firmware/shared/: the 'W' format, defaults, CRC-32

// Reported in USB device info. The build tag (FNV-1a of the build time, the
// same scheme as the panels' 'h' tag) tells two builds of one version apart.
constexpr uint8_t FW_VERSION_MAJOR = 0, FW_VERSION_MINOR = 1, FW_VERSION_PATCH = 0;
uint32_t build_tag = 0;   // computeBuildTag(), at setup

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

// ── USB pad input (docs/USB_PROTOCOL.md → "Input state report") ─────────────
// Report ID 1: panel bitmask (bit n = panel n), then a sequence number that
// steps on every state change, so the host can see a transition it missed.
// Sent from interrupt context, never from loop(): an LED burst blocks loop()
// ~7ms. The INT edge ISR qualifies and sends at once; a 1 kHz timer retries a
// send that found the queue full, repeats the report as a heartbeat, and
// corrects the mask if a line ever disagrees with it for two ticks. All three
// paths run at one NVIC priority so none preempts another.
constexpr uint32_t INT_QUALIFY_NS   = 10000;   // edge must still hold 10 us later
constexpr uint32_t HID_HEARTBEAT_US = 10000;
constexpr uint8_t  HID_IRQ_PRIORITY = 128;     // Teensy default for GPIO and USB

extern "C" volatile uint8_t usb_configuration;   // core usb.c; not in a header for this type
extern "C" volatile uint8_t usb_high_speed;      // core usb.c: set at enumeration

IntervalTimer hidTimer;
volatile uint16_t hid_mask = 0;        // qualified press state
volatile uint16_t hid_sent = 0xFFFF;   // never a real mask: forces the first report
volatile uint8_t  hid_seq = 0;
volatile uint32_t hid_last_us = 0;
volatile bool     hid_hold = false;    // 'I' self-test pulses are not presses
volatile uint32_t hid_reports = 0, hid_busy = 0, hid_glitches = 0, hid_resyncs = 0;

void hidSend() {
  uint16_t m = hid_mask;
  uint8_t seq = (m != hid_sent) ? (uint8_t)(hid_seq + 1) : hid_seq;
  uint8_t report[JOYSTICK_SIZE] = {1, (uint8_t)m, (uint8_t)(m >> 8), seq};
  if (usb_joystick_try_send(report) == 0) {
    hid_seq = seq;
    hid_sent = m;
    hid_last_us = micros();
    hid_reports++;
  } else {
    hid_busy++;
  }
}

inline void hidEdge(uint8_t panel) {
  if (hid_hold) return;
  delayNanoseconds(INT_QUALIFY_NS);
  uint16_t bit = 1u << panel;
  bool low = digitalReadFast(INT_PINS[panel]) == LOW;
  uint16_t m = low ? (hid_mask | bit) : (hid_mask & ~bit);
  if (m == hid_mask) { hid_glitches++; return; }
  hid_mask = m;
  hidSend();
}

void hidTick() {
  static uint16_t disagree = 0;   // lines that differed from the mask last tick
  if (!hid_hold) {
    uint16_t lines = 0;
    for (int i = 0; i < NUM_PANELS; i++)
      if (digitalReadFast(INT_PINS[i]) == LOW) lines |= 1u << i;
    uint16_t diff = lines ^ hid_mask;
    uint16_t fix = diff & disagree;
    disagree = diff;
    if (fix) { hid_mask ^= fix; hid_resyncs++; }
  }
  if (hid_mask != hid_sent || (micros() - hid_last_us) >= HID_HEARTBEAT_US) hidSend();
}

void hidBegin() {
  hidTimer.priority(HID_IRQ_PRIORITY);
  hidTimer.begin(hidTick, 1000);
}

void hidReport() {
  Serial.print("# USB pad input: mask 0x");
  Serial.print(hid_mask, HEX);
  Serial.print("  seq ");
  Serial.print(hid_seq);
  Serial.print("  reports ");
  Serial.print(hid_reports);
  Serial.print("  queue-full ");
  Serial.print(hid_busy);
  Serial.print("  glitches ");
  Serial.print(hid_glitches);
  Serial.print("  resyncs ");
  Serial.print(hid_resyncs);
  Serial.print("  USB ");
  Serial.print(usb_configuration ? "configured" : "NOT configured");
  Serial.println(usb_high_speed ? ", High Speed" : ", Full Speed");
}

// attachInterrupt needs a plain function pointer per pin — no capture, so one
// trampoline per panel rather than a single parameterized handler.
#define INT_TRAMPOLINE(n) void intLineIsr##n() { pushEdge(n, INT_PINS[n]); hidEdge(n); }
INT_TRAMPOLINE(0) INT_TRAMPOLINE(1) INT_TRAMPOLINE(2)
INT_TRAMPOLINE(3) INT_TRAMPOLINE(4) INT_TRAMPOLINE(5)
INT_TRAMPOLINE(6) INT_TRAMPOLINE(7) INT_TRAMPOLINE(8)
void (*const INT_ISR[NUM_PANELS])() = {
  intLineIsr0, intLineIsr1, intLineIsr2, intLineIsr3, intLineIsr4,
  intLineIsr5, intLineIsr6, intLineIsr7, intLineIsr8,
};

// ── RS-485 framing ───────────────────────────────────────────────────────────
void putU32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

uint32_t getU32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

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

// One receiver's state machine: the master's own (rx, below) and the two
// passthrough directions (pt_host, pt_bus) each have one.
struct FrameParser {
  enum { W_SYNC, W_CMD, W_ADDR, W_LEN, W_PAY, W_CRC } st = W_SYNC;
  uint8_t cmd = 0, addr = 0, len = 0, idx = 0, crc = 0;
  uint8_t pay[PKT_MAX_PAYLOAD];
  bool crc_failed = false;   // the last feed() ended a frame with a bad CRC

  // True when `c` completes a CRC-valid frame (fields above then hold it).
  bool feed(uint8_t c) {
    crc_failed = false;
    switch (st) {
      case W_SYNC: if (c == PKT_SYNC) { crc = 0; st = W_CMD; } break;
      case W_CMD:  cmd = c;  crc = crc8Update(crc, c); st = W_ADDR; break;
      case W_ADDR: addr = c; crc = crc8Update(crc, c); st = W_LEN;  break;
      case W_LEN:
        len = c; crc = crc8Update(crc, c); idx = 0;
        st = (len > PKT_MAX_PAYLOAD) ? W_SYNC : (len ? W_PAY : W_CRC);
        break;
      case W_PAY:
        pay[idx++] = c; crc = crc8Update(crc, c);
        if (idx == len) st = W_CRC;
        break;
      case W_CRC:
        st = W_SYNC;
        crc_failed = c != crc;
        return !crc_failed;
    }
    return false;
  }
};

// ── Reply window ─────────────────────────────────────────────────────────────
// After a packet that expects a reply ('F', 'W', 'H'), the master must stay silent
// until the reply is over: both ends tie DE to R̅E̅, so a master frame sent on
// top of a reply corrupts it on the wire and the master cannot even hear it.
// Nothing scheduled may transmit before this deadline. Found at the bench
// 2026-10-02: the LED burst blocks loop() ~7ms in write(), and a schedule
// computed from the pre-burst millis() fired the next poll immediately behind
// a 'C', on top of the panel's 'c' ack.
//
// The window is sized per request from the reply it expects: 10us per byte at
// 1 Mbps for the whole frame (5 bytes of framing + payload), plus a fixed
// margin for the panel's turnaround. For 'f' (14 bytes) that is the original
// 400us; 'h' (29 bytes) gets 550us.
constexpr uint32_t REPLY_MARGIN_US = 260;
constexpr uint8_t  REPLY_F_LEN = 9;    // 'f' payload
constexpr uint8_t  REPLY_W_LEN = KP_WACK_LEN;   // 'w' payload
constexpr uint8_t  REPLY_M_LEN = 2;    // 'm' echoes an addressed 'M'
constexpr uint8_t  REPLY_H_LEN = 24;   // 'h' payload (gameplay firmware)
constexpr uint8_t  REPLY_H_LEN_OLD = 22; // bring-up firmware: no parser timeouts
uint32_t bus_quiet_from_us = 0;

void sendRequest(uint8_t cmd, uint8_t addr, const uint8_t *payload, uint8_t len,
                 uint8_t reply_len) {
  rxResync();
  sendPacket(cmd, addr, payload, len);
  RS485_SERIAL.flush();   // returns at transmit-complete, when DE drops
  bus_quiet_from_us = micros() + (5u + reply_len) * 10u + REPLY_MARGIN_US;
}

bool replyWindowOver() {
  return (int32_t)(micros() - bus_quiet_from_us) >= 0;
}

// For anything that transmits outside loop()'s schedule (console commands).
// Pausing bus_traffic stops new traffic but not what is already on the wire: a
// queued LED burst, or the reply to the last poll. Found at the bench
// 2026-10-19: 'I' sent straight after a poll landed on the 'f' reply, and the
// self-test lost IDs in 6 of 10 runs with traffic running, 0 of 10 paused.
void waitBusQuiet() {
  RS485_SERIAL.flush();                // our own TX out, DE dropped
  while (!replyWindowOver()) {}        // any reply that was owed has finished
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
// The panel went offline or came back during this window. A window like that
// says nothing about collisions, so it is never judged (checkPanelHealth()).
bool     presence_changed_window[NUM_PANELS] = {};
// Lifetime per-panel reply count. The windowed counters reset every heartbeat,
// so they cannot answer "has this panel EVER answered" — which is what decides
// whether a panel is worth printing a line for.
uint32_t poll_ok_total[NUM_PANELS] = {};
// Presence tracking. `poll_ok_total` is a LIFETIME counter, so on its own it can
// only ever say "this panel replied at some point" — which made the heartbeat
// keep printing a panel's last-known FSR values as though they were live long
// after it stopped answering. A panel that is unplugged, reset, or dead must
// read as gone, not as frozen. Found at the bench 2026-09-08 by toggling the
// panel's responder off and watching the master carry on regardless.
// Updated as replies arrive and by updatePresence() every loop(), so presence
// changes are seen at once (console, USB events, config pushes).
constexpr uint32_t PANEL_TIMEOUT_MS = 1000;   // ~14 missed polls at the measured ~14 Hz
uint32_t last_reply_ms[NUM_PANELS] = {};
bool     panel_online[NUM_PANELS]  = {};
// Poll reply rate over the last completed heartbeat window, for USB pad status.
uint8_t  poll_pct[NUM_PANELS] = {};

// Latest 'h' from each panel. The console's `H` sweep fills it, and so does a
// background request to one answering panel every STATUS_BG_MS, for USB pad
// status and the termination check in device info.
constexpr uint32_t STATUS_BG_MS = 500;
struct PanelStatus { bool got; uint32_t build_tag; uint16_t crc_errs; bool term; };
PanelStatus panel_status[NUM_PANELS] = {};

extern volatile int ident_ack_id;   // defined with the bench checks below

// 'H' status sweep: one request per poll slot, IDs 0-8, started by `H`.
// A panel that has not answered by the time the next slot comes round is
// reported as silent then, so the sweep needs no timer of its own.
int  status_next = -1;              // next index to ask; -1 = no sweep running
bool status_got[NUM_PANELS] = {};
bool status_term[NUM_PANELS] = {};  // SW202 position from each 'h'
bool status_verbose = false;        // print 'h' replies: `H` sweep and `b` test only
int  bg_status_due = -1;            // panel owed a background 'H', -1 = none

// RS-485 daisy-chain order, master first (CLAUDE.md -> "Daisy-Chain Order").
// Documented, not measured: the bus cannot discover its own topology, so a pad
// cabled in another order gets the wrong expected terminator.
constexpr uint8_t CHAIN_ORDER[NUM_PANELS] = {0, 3, 6, 7, 4, 1, 2, 5, 8};

// After an 'H' sweep: exactly one panel should be terminated, the last one in
// chain order that answered (R3 terminates the master end). A WARNING, never an
// error: on the bench 2026-10-18 the bus ran ~100k frames per panel with zero
// errors unterminated, terminated mid-chain, at the wrong end, and doubled to
// 40 ohm (docs/BRINGUP_LOG.md, master #2). Limits: the bit is SW202 pole B
// only, so a missing R201 or a bad pole A still reads ON; and a panel that is
// cabled in but silent (unpowered, dead) still passes A/B through, so the real
// end of the cable may lie beyond the last panel that answered.
// The rule, over any set of 'h' results: 0 OK, 1 none terminated, 2 several,
// 3 the wrong panel, 0xFF nothing to judge (no status yet). `term_id` gets the
// terminated panel's ID when exactly one is, else 0xFF. USB device info
// reports exactly this.
uint8_t terminationCode(const bool *got, const bool *term, uint8_t *term_id) {
  int expect = -1, n_term = 0, which = -1;
  for (int k = 0; k < NUM_PANELS; k++)
    if (got[CHAIN_ORDER[k]]) expect = CHAIN_ORDER[k];
  *term_id = 0xFF;
  if (expect < 0) return 0xFF;
  for (int i = 0; i < NUM_PANELS; i++)
    if (got[i] && term[i]) { n_term++; which = i; }
  if (n_term == 0) return 1;
  if (n_term > 1) return 2;
  *term_id = PANEL_IDS[which];
  return which == expect ? 0 : 3;
}

// The console's version, over the last `H` sweep, with the fix spelled out.
void checkTermination() {
  int expect = -1, n_term = 0;
  for (int k = 0; k < NUM_PANELS; k++)
    if (status_got[CHAIN_ORDER[k]]) expect = CHAIN_ORDER[k];
  if (expect < 0) return;
  for (int i = 0; i < NUM_PANELS; i++) n_term += status_got[i] && status_term[i];

  if (n_term == 1 && status_term[expect]) {
    Serial.print("# termination OK: ");
    Serial.print(PANEL_NAME[expect]);
    Serial.print(" (id ");
    Serial.print(PANEL_IDS[expect]);
    Serial.println("), the last panel answering in chain order");
    return;
  }
  Serial.print("# termination WARNING: ");
  if (n_term == 0) {
    Serial.print("no panel terminated");
  } else {
    Serial.print(n_term);
    Serial.print(n_term == 1 ? " panel terminated:" : " panels terminated:");
    for (int i = 0; i < NUM_PANELS; i++) {
      if (!(status_got[i] && status_term[i])) continue;
      Serial.print(' ');
      Serial.print(PANEL_NAME[i]);
    }
  }
  Serial.print(". Expected only ");
  Serial.print(PANEL_NAME[expect]);
  Serial.print(" (id ");
  Serial.print(PANEL_IDS[expect]);
  Serial.println("), the last panel answering in chain order. SW202 on that panel ON, every other off");
}

void storeStatusReply(int pidx, const uint8_t *p) {
  panel_status[pidx].got = true;
  panel_status[pidx].build_tag = getU32(p);
  panel_status[pidx].crc_errs = (uint16_t)p[10] | ((uint16_t)p[11] << 8);
  panel_status[pidx].term = p[9] & 0x02;
}

void printStatusReply(uint8_t addr, const uint8_t *p, uint8_t len) {
  int pidx = panelIndex(addr);
  uint32_t tag    = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  uint32_t up_s   = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
  uint8_t  rst    = p[8];
  uint8_t  st     = p[9];
  uint16_t crc    = (uint16_t)p[10] | ((uint16_t)p[11] << 8);
  uint16_t uart   = (uint16_t)p[12] | ((uint16_t)p[13] << 8);
  uint16_t ovr    = (uint16_t)p[14] | ((uint16_t)p[15] << 8);
  uint16_t turn   = (uint16_t)p[16] | ((uint16_t)p[17] << 8);
  uint32_t frames = (uint32_t)p[18] | ((uint32_t)p[19] << 8) | ((uint32_t)p[20] << 16) | ((uint32_t)p[21] << 24);
  if (pidx >= 0) {
    status_got[pidx] = true;
    status_term[pidx] = st & 0x02;
  }

  char tagbuf[9];
  snprintf(tagbuf, sizeof(tagbuf), "%08lX", (unsigned long)tag);
  Serial.print("# status ");
  Serial.print(pidx >= 0 ? PANEL_NAME[pidx] : "?");
  Serial.print("\t id ");
  Serial.print(addr);
  Serial.print("  build ");
  Serial.print(tagbuf);
  Serial.print("  up ");
  Serial.print(up_s);
  Serial.print("s  reset");
  if (rst & 0x01) Serial.print(" power-on");
  if (rst & 0x02) Serial.print(" RUN-pin");
  if (rst & 0x04) Serial.print(" debugger/PSM");
  if (rst & 0x08) Serial.print(" watchdog-timeout");
  if (rst & 0x10) Serial.print(" watchdog-forced");
  if (!rst)       Serial.print(" none-flagged");
  Serial.print("  12V ");
  Serial.print((st & 0x01) ? "yes" : "NO");
  Serial.print("  term ");
  Serial.print((st & 0x02) ? "ON" : "off");
  Serial.print("  autoINT ");
  Serial.print((st & 0x04) ? "on" : "OFF");
  Serial.print("  echo ");
  Serial.print((st & 0x08) ? "on" : "OFF");
  Serial.print("  INT ");
  Serial.print((st & 0x10) ? "asserted" : "idle");
  // Bits 5-6: LED mode on the gameplay firmware; always 0 on the bring-up build.
  if (((st >> 5) & 0x03) == 1) Serial.print("  LEDs ID");
  if (((st >> 5) & 0x03) == 2) Serial.print("  LEDs test");
  Serial.print("  | frames ");
  Serial.print(frames);
  Serial.print(" crc ");
  Serial.print(crc);
  Serial.print(" uart ");
  Serial.print(uart);
  Serial.print(" ovr ");
  Serial.print(ovr);
  Serial.print(" turn ");
  Serial.print(turn);
  if (len >= REPLY_H_LEN) {
    Serial.print(" tmo ");
    Serial.print((uint16_t)p[22] | ((uint16_t)p[23] << 8));
  }
  Serial.println();
}

// ── Panel config: the master's copy, its EEPROM record, each panel's state ───
// The master holds the only saved copy of every panel's config; panels keep
// theirs in RAM and boot on defaults (docs/PANEL_CONFIG.md → "Config
// persistence"). This copy is what panels are sent and what USB Get config
// returns. The console's `S` and `F` edit it without saving (`save` saves);
// USB Set config and both resets save it at once.
//
// A panel is sent its 'W' in a poll slot: when it appears (which covers the
// master's own boot), when its 'f' says "not configured since boot" after it
// had matched (it rebooted), when the copy changes, and as a re-push, one
// panel every CFG_REPUSH_MS / 9 so each gets it about once a second. Its 'w'
// carries the CRC of the config it now runs; only a CRC equal to the master's
// copy counts as configured.
constexpr uint16_t CFG_BLOCK_LEN = NUM_PANELS * KP_PANEL_CFG_LEN;   // 180
constexpr uint32_t CFG_REPUSH_MS = 1000;
uint8_t  cfg_block[CFG_BLOCK_LEN];
bool     cfg_unsaved = false;      // edited since the last load or save
bool     cfg_was_reset = false;    // no valid record at boot: defaults until saved
uint16_t cfg_seq = 0;              // sequence number of the newest record
int      cfg_slot = -1;            // slot holding it, -1 = none

// The EEPROM record (docs/PANEL_CONFIG.md → "The master's EEPROM record"):
// magic, version, sequence, the block, then the CRC-32 of all of that. Two
// slots; a save writes the one not holding the newest record, so a power cut
// mid-save leaves the previous config intact.
constexpr uint8_t  CFG_MAGIC[4] = {'K', 'P', 'C', 'F'};
constexpr uint16_t CFG_REC_SEQ = 5, CFG_REC_BLOCK = 7, CFG_REC_CRC = CFG_REC_BLOCK + CFG_BLOCK_LEN;
constexpr uint16_t CFG_REC_LEN = CFG_REC_CRC + 4;   // 191
constexpr uint16_t CFG_SLOT_ADDR[2] = {0, 512};
static_assert(CFG_SLOT_ADDR[1] + CFG_REC_LEN <= E2END + 1, "slot B must fit the emulated EEPROM");

// Per panel, numbered as the USB config result event reports it.
enum : uint8_t {
  CFG_MATCH = 0, CFG_WAITING = 1, CFG_REJECTED_VERSION = 2,
  CFG_REJECTED_INVALID = 3, CFG_NOT_CONFIGURED = 4, CFG_ABSENT = 5,
};
const char *const CFG_STATE_NAME[] = {
  "matches", "sent, waiting for the panel", "REJECTED: unknown config version",
  "REJECTED: invalid values", "not configured since boot", "absent",
};
uint8_t  cfg_state[NUM_PANELS];
uint32_t cfg_sent_crc[NUM_PANELS] = {};    // CRC of the last 'W' sent
uint32_t cfg_panel_crc[NUM_PANELS] = {};   // CRC the panel reported in its last 'w'
uint8_t  cfg_panel_ver[NUM_PANELS] = {};   // highest version it speaks, from 'w'
bool     cfg_push_due[NUM_PANELS] = {};
uint32_t cfg_mismatches = 0;               // 'w' said applied, CRC was not what was sent
uint32_t cfg_save_us = 0;                  // how long the last save took

uint8_t *cfgPanel(int pidx) { return cfg_block + pidx * KP_PANEL_CFG_LEN; }

void cfgDefaults() {
  for (int i = 0; i < NUM_PANELS; i++) kp_panel_cfg_default(cfgPanel(i));
}

// Prints the moments that matter: a panel confirmed running the master's copy
// (with its thresholds, which tools/fsr_monitor.py reads as the panel's ack),
// and a rejection.
void cfgSetState(int pidx, uint8_t st) {
  if (cfg_state[pidx] == st) return;
  cfg_state[pidx] = st;
  if (st == CFG_MATCH) {
    Serial.print("# panel ");
    Serial.print(PANEL_IDS[pidx]);
    Serial.print(" (");
    Serial.print(PANEL_NAME[pidx]);
    Serial.print(") config matches:");
    for (int c = 0; c < KP_NUM_FSR; c++) {
      const uint8_t *ch = cfgPanel(pidx) + c * KP_CH_LEN;
      Serial.print(' ');
      Serial.print(c);
      Serial.print(':');
      Serial.print(kp_get_u16(ch));
      Serial.print('/');
      Serial.print(kp_get_u16(ch + 2));
    }
    Serial.println();
  } else if (st == CFG_REJECTED_VERSION || st == CFG_REJECTED_INVALID) {
    Serial.print("# !! panel ");
    Serial.print(PANEL_IDS[pidx]);
    Serial.print(" (");
    Serial.print(PANEL_NAME[pidx]);
    Serial.print(") rejected its config: ");
    if (st == CFG_REJECTED_VERSION) {
      Serial.print("it speaks config version ");
      Serial.print(cfg_panel_ver[pidx]);
      Serial.print(", this master sends ");
      Serial.print(KP_CONFIG_VERSION);
      Serial.println(". Update the panel firmware");
    } else {
      Serial.println("invalid values (a master bug: the copy is checked before it is sent)");
    }
  }
  vndConfigResultEvent(pidx);
}

// The copy changed: every panel is owed it, and one that matched no longer does.
void cfgChanged() {
  for (int i = 0; i < NUM_PANELS; i++) {
    cfg_push_due[i] = true;
    if (cfg_state[i] == CFG_MATCH) cfgSetState(i, CFG_WAITING);
  }
}

bool cfgReadSlot(int slot, uint8_t *rec) {
  for (uint16_t i = 0; i < CFG_REC_LEN; i++) rec[i] = EEPROM.read(CFG_SLOT_ADDR[slot] + i);
  if (memcmp(rec, CFG_MAGIC, 4) != 0 || getU32(rec + CFG_REC_CRC) != kp_crc32_from(0, rec, CFG_REC_CRC))
    return false;
  // Version 1 is the only one so far, so there is nothing older to convert.
  if (rec[4] != KP_CONFIG_VERSION) return false;
  for (int i = 0; i < NUM_PANELS; i++)
    if (kp_panel_cfg_bad_channel(rec + CFG_REC_BLOCK + i * KP_PANEL_CFG_LEN) >= 0) return false;
  return true;
}

// At boot: the valid record with the higher sequence number wins. Neither
// valid → defaults, flagged as "config was reset" until something is saved.
void cfgLoad() {
  static uint8_t rec[2][CFG_REC_LEN];
  bool ok[2] = {cfgReadSlot(0, rec[0]), cfgReadSlot(1, rec[1])};
  int pick = -1;
  if (ok[0] && ok[1])
    pick = (int16_t)(kp_get_u16(rec[1] + CFG_REC_SEQ) - kp_get_u16(rec[0] + CFG_REC_SEQ)) > 0 ? 1 : 0;
  else if (ok[0] || ok[1])
    pick = ok[0] ? 0 : 1;
  if (pick < 0) {
    cfgDefaults();
    cfg_was_reset = true;
    return;
  }
  memcpy(cfg_block, rec[pick] + CFG_REC_BLOCK, CFG_BLOCK_LEN);
  cfg_seq = kp_get_u16(rec[pick] + CFG_REC_SEQ);
  cfg_slot = pick;
}

// Writes the copy to the slot not holding the newest record, then reads it
// back. Every byte the emulation actually changes holds off all interrupts
// while the flash programs, and an occasional sector erase holds them off for
// the whole erase, so INT, USB and the bus stall in bursts: never during
// gameplay (docs/PANEL_CONFIG.md).
//
// Timed with the cycle counter, not micros(): micros() is built on the
// millisecond tick, whose interrupt the writes hold off.
bool cfgSave() {
  uint32_t c0 = ARM_DWT_CYCCNT;
  uint8_t rec[CFG_REC_LEN];
  uint16_t seq = cfg_seq + 1;
  memcpy(rec, CFG_MAGIC, 4);
  rec[4] = KP_CONFIG_VERSION;
  kp_put_u16(rec + CFG_REC_SEQ, seq);
  memcpy(rec + CFG_REC_BLOCK, cfg_block, CFG_BLOCK_LEN);
  putU32(rec + CFG_REC_CRC, kp_crc32_from(0, rec, CFG_REC_CRC));
  int slot = (cfg_slot == 0) ? 1 : 0;
  for (uint16_t i = 0; i < CFG_REC_LEN; i++) EEPROM.update(CFG_SLOT_ADDR[slot] + i, rec[i]);
  cfg_save_us = (ARM_DWT_CYCCNT - c0) / (F_CPU_ACTUAL / 1000000);
  for (uint16_t i = 0; i < CFG_REC_LEN; i++)
    if (EEPROM.read(CFG_SLOT_ADDR[slot] + i) != rec[i]) return false;
  cfg_slot = slot;
  cfg_seq = seq;
  cfg_unsaved = false;
  cfg_was_reset = false;
  return true;
}

// A whole new copy from USB or a reset: saved, then pushed to every panel.
bool cfgCommit() {
  cfg_unsaved = true;
  bool ok = cfgSave();
  cfgChanged();
  return ok;
}

// The first answering panel owed a 'W', or -1.
int cfgNextPush() {
  for (int i = 0; i < NUM_PANELS; i++)
    if (cfg_push_due[i] && panel_online[i]) return i;
  return -1;
}

void cfgSendPush(int pidx) {
  uint8_t w[KP_W_LEN];
  w[0] = KP_CONFIG_VERSION;
  memcpy(w + 1, cfgPanel(pidx), KP_PANEL_CFG_LEN);
  sendRequest('W', PANEL_IDS[pidx], w, sizeof(w), REPLY_W_LEN);
  cfg_sent_crc[pidx] = kp_w_crc(cfgPanel(pidx));
  cfg_push_due[pidx] = false;
  if (cfg_state[pidx] == CFG_NOT_CONFIGURED) cfgSetState(pidx, CFG_WAITING);
}

void cfgAck(int pidx, const uint8_t *p) {
  uint32_t crc = getU32(p + 2);
  cfg_panel_ver[pidx] = p[1];
  cfg_panel_crc[pidx] = crc;
  if (p[0] == KP_WACK_UNKNOWN_VERSION) {
    cfgSetState(pidx, CFG_REJECTED_VERSION);
  } else if (p[0] != KP_WACK_APPLIED) {
    cfgSetState(pidx, CFG_REJECTED_INVALID);
  } else if (crc == kp_w_crc(cfgPanel(pidx))) {
    cfgSetState(pidx, CFG_MATCH);
  } else if (crc != cfg_sent_crc[pidx]) {
    // It applied something, but not what was sent. Try again.
    cfg_mismatches++;
    cfg_push_due[pidx] = true;
    cfgSetState(pidx, CFG_WAITING);
    Serial.print("# !! panel ");
    Serial.print(PANEL_IDS[pidx]);
    Serial.println(" acked 'W' with a CRC that is not what was sent: re-sending");
  }
  // Otherwise it applied what was sent, but the copy has changed since; that
  // push is already due.
}

// Every 'f' reply. A panel appearing gets its config; one that matched and now
// says "not configured since boot" rebooted, and gets it again at once.
void panelSeen(int pidx, bool not_configured) {
  if (!panel_online[pidx]) {
    panel_online[pidx] = true;
    presence_changed_window[pidx] = true;
    Serial.print("# panel ");
    Serial.print(PANEL_IDS[pidx]);
    Serial.print(" (");
    Serial.print(PANEL_NAME[pidx]);
    Serial.println(") is replying");
    vndPresenceEvent(pidx, true);
    cfg_push_due[pidx] = true;
    cfgSetState(pidx, not_configured ? CFG_NOT_CONFIGURED : CFG_WAITING);
  } else if (not_configured && cfg_state[pidx] == CFG_MATCH) {
    Serial.print("# panel ");
    Serial.print(PANEL_IDS[pidx]);
    Serial.print(" (");
    Serial.print(PANEL_NAME[pidx]);
    Serial.println(") is on defaults again (rebooted?): re-sending its config");
    cfg_push_due[pidx] = true;
    cfgSetState(pidx, CFG_NOT_CONFIGURED);
  }
}

void updatePresence(uint32_t now_ms) {
  for (int i = 0; i < NUM_PANELS; i++) {
    if (!panel_online[i] || (now_ms - last_reply_ms[i]) < PANEL_TIMEOUT_MS) continue;
    panel_online[i] = false;
    presence_changed_window[i] = true;
    Serial.print("# !! panel ");
    Serial.print(PANEL_IDS[i]);
    Serial.print(" (");
    Serial.print(PANEL_NAME[i]);
    Serial.println(") STOPPED REPLYING -- unplugged, reset, or dead");
    vndPresenceEvent(i, false);
    cfgSetState(i, CFG_ABSENT);
  }
}

void printConfig() {
  Serial.print("# config, version ");
  Serial.print(KP_CONFIG_VERSION);
  if (cfg_slot >= 0) {
    Serial.print(", saved in slot ");
    Serial.print(cfg_slot ? 'B' : 'A');
    Serial.print(" (sequence ");
    Serial.print(cfg_seq);
    Serial.print(")");
  } else {
    Serial.print(", nothing saved");
  }
  if (cfg_was_reset) Serial.print(", RESET at boot: no valid saved config, on defaults");
  if (cfg_unsaved) Serial.print(", UNSAVED edits (`save` keeps them)");
  Serial.println();
  Serial.println("  channels 0-3 in 'f' order: press/release, then p = not populated, d = disabled");
  for (int i = 0; i < NUM_PANELS; i++) {
    Serial.print("  ");
    Serial.print(PANEL_NAME[i]);
    Serial.print("\t id ");
    Serial.print(PANEL_IDS[i]);
    for (int c = 0; c < KP_NUM_FSR; c++) {
      const uint8_t *ch = cfgPanel(i) + c * KP_CH_LEN;
      char buf[24];
      snprintf(buf, sizeof(buf), "  %d:%4u/%4u%s%s", c, kp_get_u16(ch), kp_get_u16(ch + 2),
               (ch[4] & KP_CH_POPULATED) ? "" : " p", (ch[4] & KP_CH_ENABLED) ? "" : " d");
      Serial.print(buf);
    }
    Serial.print("   ");
    Serial.println(CFG_STATE_NAME[cfg_state[i]]);
  }
  if (cfg_mismatches) {
    Serial.print("  'w' CRC mismatches since boot: ");
    Serial.println(cfg_mismatches);
  }
  if (cfg_save_us) {
    Serial.print("  last save took ");
    Serial.print(cfg_save_us);
    Serial.println(" us");
  }
}

// The master's receiver for panel replies. Passthrough reads the bus through
// pt_bus instead, and leaves this one alone.
FrameParser rx;
uint32_t stat_rx_dropped = 0;   // partial packets dropped by rxResync()

void parseRx() {
  while (RS485_SERIAL.available()) {
    if (!rx.feed((uint8_t)RS485_SERIAL.read())) {
      if (rx.crc_failed) stat_crc_errs++;
      continue;
    }
    uint8_t cmd = rx.cmd, addr = rx.addr, len = rx.len;
    const uint8_t *pay = rx.pay;
    int pidx = panelIndex(addr);
    if (cmd == 'f' && len == 9 && pidx >= 0) {
      for (int i = 0; i < 4; i++)
        panel_fsr[pidx][i] = (uint16_t)pay[2 * i] | ((uint16_t)pay[2 * i + 1] << 8);
      panel_pressed_mask[pidx] = pay[8] & KP_F_PRESSED_MASK;
      stat_replies++;
      poll_ok_window[pidx]++;
      poll_ok_total[pidx]++;
      last_reply_ms[pidx] = millis();
      panelSeen(pidx, pay[8] & KP_F_NOT_CONFIGURED);
      vndStreamRecord(addr, micros(), pay);
    } else if (cmd == 'h' && (len == REPLY_H_LEN || len == REPLY_H_LEN_OLD)) {
      if (pidx >= 0) storeStatusReply(pidx, pay);
      if (status_verbose) printStatusReply(addr, pay, len);
    } else if (cmd == 'w' && len == KP_WACK_LEN && pidx >= 0) {
      cfgAck(pidx, pay);
    } else if (cmd == 'i' && len == 0) {
      ident_ack_id = addr;
    }
  }
}

// The master's replacement for the panels' 500 us inter-byte timeout. It can't
// time bytes itself: the core's interrupt buffers them and loop() reads them
// late (an LED burst blocks it ~7 ms). It doesn't need to. The master owns the
// bus, every reply answers one of its requests, and it transmits again only
// once that request's reply window has closed, by which time the whole reply
// is readable (available() counts the UART's FIFO too). So a packet still
// open when the master is about to transmit is never a reply in progress: it
// is garbage that parsed as a header (a stray 0x55, then a length), or a reply
// cut short, and left alone it would swallow the next replies as its payload.
// Called before every scheduled transmission; drops such a packet and counts
// it (heartbeat, `z` zeroes it).
void rxResync() {
  parseRx();
  if (rx.st == FrameParser::W_SYNC) return;
  rx.st = FrameParser::W_SYNC;
  stat_rx_dropped++;
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
// `w`: every LED on every panel at full white (255,255,255), presses ignored,
// for the full-white soak. Load is ~0.44 A per panel at 12 V; nine panels
// ~4 A (hardware/harness/README.md -> "Power budget").
bool white_soak = false;

// A reply-bearing command must not go out the moment it is typed: it can land
// tens of microseconds after an 'F' poll, while that panel is still sending
// its 'f'. The panel's DE and R̅E̅ are tied, so it hears none of the command
// while it transmits, and the two collide on the wire. Seen at the bench
// 2026-10-01 with the old 'C': the panel captured `55 43 01 05 FF FE` and then
// the next poll's header as payload, and the master logged CRC errors for the
// same moments. So everything below is queued, and loop() sends it IN PLACE OF
// the next poll, in a scheduled slot with the reply window that comes with it.
// Config pushes ('W') work the same way (cfg_push_due).
// `z`: zero the bus counters here and, by broadcast 'Z', on every panel.
bool    clear_pending = false;
// `m` and USB: panel LED mode (0 normal, 1 ID, 2 sensor test). Every panel is
// one broadcast 'M'; a subset is one addressed 'M' per panel, each acked.
bool     mode_pending = false;
uint16_t mode_mask_pending = 0;     // panel IDs still owed an addressed 'M'
uint8_t  mode_payload[2];

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

// `n` reads the INT lines with the Teensy's own pull-ups enabled, so it CANNOT
// see a MISSING external pull-up: with RN1 unsoldered every line still reads
// HIGH, held by the internal pull-up instead. That matters because RN1 is a
// hand-soldered SIP-10 whose pin 1 is the common feeding all nine resistors —
// one cold joint there silently removes all nine external pull-ups at once, and
// the 10k value is deliberate (docs/MASTER_PCB.md: "deliberately 10k not stiff",
// the INT-into-dead-panel case).
//
// This inverts the bias instead. With the internal pull-DOWN selected (~100k on
// i.MX RT) RN1's 10k to +3.3VDC still wins the divider — 3.3 * 100/110 ~= 3.0V,
// a solid HIGH — while a line with no external pull-up is dragged to 0V. RN1
// lands on the Teensy-side node (same net as C3-C11), so the settling RC is
// 100k * 1nF = 100us worst case; 1ms is 10 time constants.
//
//   all nine HIGH  RN1 present and pulling on every line
//   any line LOW   that line has no external pull-up: RN1 joint, or open trace
//   all nine LOW   no external pull-ups at all — RN1's common pin 1 is open, or
//                  the Teensy is not seated on the master board
//
// Nothing may be plugged into J3-J11 while this runs: a panel holding its
// open-drain INT low is indistinguishable from a missing pull-up.
void reportIntPullups() {
  Serial.println("INT external pull-up (RN1) check.");
  Serial.println("Nothing may be plugged into J3-J11 — a held INT reads as a missing pull-up.");

  hid_hold = true;   // a pin with no pull-up reads LOW here: not a press
  for (int i = 0; i < NUM_PANELS; i++)
    detachInterrupt(digitalPinToInterrupt(INT_PINS[i]));

  bool pulled[NUM_PANELS];
  for (int i = 0; i < NUM_PANELS; i++) {
    pinMode(INT_PINS[i], INPUT_PULLDOWN);
    delayMicroseconds(1000);
    pulled[i] = (digitalReadFast(INT_PINS[i]) == HIGH);
    pinMode(INT_PINS[i], INPUT_PULLUP);
  }

  // Those mode changes toggled every pin, so the ring now holds phantom edges.
  // Drop them before the ISRs go back on, or they surface as nine fake presses.
  noInterrupts();
  ring_head = 0;
  ring_tail = 0;
  interrupts();
  for (int i = 0; i < NUM_PANELS; i++)
    attachInterrupt(digitalPinToInterrupt(INT_PINS[i]), INT_ISR[i], CHANGE);
  hid_hold = false;

  int missing = 0;
  for (int i = 0; i < NUM_PANELS; i++) {
    if (!pulled[i]) missing++;
    Serial.print("  ");
    Serial.print(PANEL_NAME[i]);
    Serial.print("\t panel ");
    Serial.print(PANEL_IDS[i]);
    Serial.print("\t pin ");
    Serial.print(INT_PINS[i]);
    Serial.print("\t ");
    Serial.print(INT_CONN[i]);
    Serial.print("\t RN1.");
    Serial.print(i + 2);            // RN1 pin 1 is the common; .2 = UL
    Serial.println(pulled[i] ? "\t OK   (external pull-up present)"
                             : "\t ??   NO external pull-up");
  }

  if (missing == 0) {
    Serial.println("  => PASS: all nine external pull-ups present.");
  } else if (missing == NUM_PANELS) {
    Serial.println("  => ALL NINE MISSING. Either RN1 pin 1 (the common, to +3.3VDC)");
    Serial.println("     is open, or the Teensy is not seated on the master board.");
  } else {
    Serial.print("  => ");
    Serial.print(missing);
    Serial.println(" line(s) missing an external pull-up: reflow that RN1 pin,");
    Serial.println("     then check the trace from it to the Teensy pin.");
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
    ? "  => CONFIRMED: all nine on one register. A single read samples the pad;\r\n"
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

// The strip takes its bytes in BRG order, not RGB — measured on the first
// strip tested 2026-10-05: sending R,G,B lit it blue, red, green. Callers pass
// r,g,b; the reorder happens here only.
void underglowFill(uint8_t r, uint8_t g, uint8_t b) {
  noInterrupts();
  for (int i = 0; i < UNDERGLOW_GROUPS; i++) {
    underglowSendByte(b); underglowSendByte(r); underglowSendByte(g);
  }
  interrupts();
  delayMicroseconds(300);   // latch
}

// `u` sends real WS2811 frames, which a handheld DMM cannot see: ~1.3 ms of
// burst per frame with the line parked LOW between, so a meter reads ~5 mV and
// looks like a dead pin. The meter-readable `U` (steady 6 s dwells + R4 check)
// was removed 2026-10-22 once the strips were installed; it is in git history
// for bringing up another master (docs/MASTER_BRINGUP.md).
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

// One group lit, every other group dark. `group` is 0-based; -1 blanks all.
// Dim white, so the byte order (BRG, see underglowFill) does not matter here.
void underglowSingle(int group) {
  noInterrupts();
  for (int i = 0; i < UNDERGLOW_GROUPS; i++) {
    uint8_t v = (i == group) ? 20 : 0;
    underglowSendByte(v); underglowSendByte(v); underglowSendByte(v);
  }
  interrupts();
  delayMicroseconds(300);   // latch
}

// `g` walks one lit group from the data-in end to the far end. The fill tests
// (`u`) cannot see a group that passes data on but has a dead output, or two
// groups swapped in the run; this can. Groups are numbered from 1 at the
// connector, so the printed number is the one you count on the strip.
// `g <n>` lights group n alone and holds it, for finding a fault by eye.
void underglowChase() {
  Serial.println("# underglow chase: one group at a time, 1 = nearest the connector");
  for (int i = 0; i < UNDERGLOW_GROUPS; i++) {
    Serial.print("  group "); Serial.println(i + 1);
    underglowSingle(i);
    delay(300);
  }
  underglowSingle(-1);
  Serial.println("# done, strip blanked. A dark group, or two lit at once, is the fault.");
}

// ── Slot <-> panel-ID self-test (docs/RS485_PROTOCOL.md) ─────────────────────
// The master half of 'I'. The bring-up and gameplay panel firmwares both
// answer it.
volatile int  ident_ack_id = -1;

// Bench test for the panel parser's inter-byte timeout (2026-10-20). Sends a
// lone header claiming an 80-byte payload, goes quiet, then asks every panel
// for 'h'. A parser with no timeout reads the next 81 bytes (all nine 'H'
// requests) as that payload and answers none; one with it answers all nine
// and reports a parser timeout.
void brokenHeaderTest() {
  constexpr uint32_t GAP_US = 2000;   // well past the panels' 500 us

  bool was = bus_traffic;
  bus_traffic = false;
  waitBusQuiet();
  parseRx();
  for (int i = 0; i < NUM_PANELS; i++) status_got[i] = false;
  status_verbose = true;
  Serial.println("# broken-header test: header claiming 80 bytes, then 'H' to each panel");

  const uint8_t hdr[4] = {PKT_SYNC, 'X', BROADCAST, PKT_MAX_PAYLOAD};
  RS485_SERIAL.write(hdr, sizeof(hdr));
  RS485_SERIAL.flush();
  delayMicroseconds(GAP_US);

  for (int i = 0; i < NUM_PANELS; i++) {
    sendRequest('H', PANEL_IDS[i], nullptr, 0, REPLY_H_LEN);
    while (!replyWindowOver()) parseRx();
    parseRx();
  }

  status_verbose = false;
  int n = 0;
  for (int i = 0; i < NUM_PANELS; i++) n += status_got[i];
  Serial.print("# broken-header test: ");
  Serial.print(n);
  Serial.print("/9 answered -> ");
  Serial.println(n == NUM_PANELS ? "PASS (each line above should show tmo >= 1)"
                                 : "FAIL (a panel parser is still inside the bogus packet)");
  bus_traffic = was;
}

// Results for USB (Slot self-test): slot_id[s] = the panel ID whose pulse
// arrived on INT slot s (0xFF none), acked = bit per ID that answered 'i',
// low = bit per slot found already LOW by the pre-check. False if the
// pre-check stopped it.
bool identifySelfTest(uint8_t *slot_id, uint16_t *acked_mask, uint16_t *low_mask) {
  constexpr uint8_t PULSE_MS = 2;
  constexpr uint32_t ACK_TIMEOUT_MS = 20;
  constexpr uint32_t EDGE_WINDOW_MS = 50;

  for (int i = 0; i < NUM_PANELS; i++) slot_id[i] = 0xFF;
  *acked_mask = *low_mask = 0;

  // Step 1: idle pre-check. A line already LOW cannot show a transition, so
  // mis-attributing a later pulse is the real risk — stop instead.
  bool blocked = false;
  for (int i = 0; i < NUM_PANELS; i++) {
    if (digitalReadFast(INT_PINS[i]) == LOW) {
      *low_mask |= 1u << i;
      Serial.print("# pre-check FAIL: ");
      Serial.print(PANEL_NAME[i]);
      Serial.print(" (pin ");
      Serial.print(INT_PINS[i]);
      Serial.println(") is already LOW — stood-on panel, stuck FSR, or shorted wire");
      blocked = true;
    }
  }
  if (blocked) { Serial.println("# aborting self-test"); return false; }

  bool was = bus_traffic;
  bus_traffic = false;   // no LED frames or polls competing for the bus
  waitBusQuiet();        // ...including whatever is already on the wire
  parseRx();             // and consume that last reply before acks are awaited
  hid_hold = true;       // the pulses must not reach the PC as presses
  Serial.println("# slot <-> panel-ID self-test");

  for (int id = 0; id < NUM_PANELS; id++) {
    ident_ack_id = -1;
    uint8_t payload[1] = {PULSE_MS};
    rxResync();
    sendPacket('I', (uint8_t)id, payload, 1);

    uint32_t t0 = millis();
    while (ident_ack_id != id && (millis() - t0) < ACK_TIMEOUT_MS) parseRx();
    bool acked = (ident_ack_id == id);
    if (acked) *acked_mask |= 1u << id;

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

    for (int k = 0; k < nfired; k++) slot_id[fired[k]] = (uint8_t)id;

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
  hid_hold = false;      // the 1 kHz tick re-reads the lines within 2 ms
  Serial.println("# self-test done");
  return true;
}

// ── Passthrough (panel firmware updates) ─────────────────────────────────────
// `P` turns the master into a dumb bridge for tools/panel_update.py: frames in
// the docs/RS485_PROTOCOL.md format arriving on USB are checked and sent to the
// bus unchanged, and every CRC-valid frame heard on the bus goes back to USB
// unchanged. The master's own traffic (LED frames, polls) and all text output
// stop, so the USB stream is pure frames. The update logic lives on the host.
//
// cmd 0x00 frames are for the master itself and never reach the bus:
//   addr 0  leave passthrough        addr 1  ping, echoed back to the host
// It also leaves after PT_IDLE_MS with nothing from the host, so a crashed
// host tool cannot leave the pad dark. docs/FIELD_UPDATE.md has the rest.
constexpr uint32_t PT_IDLE_MS = 30000;
bool     passthrough = false;
uint32_t pt_last_host_ms = 0;

FrameParser pt_host, pt_bus;

void ptWriteHost(uint8_t cmd, uint8_t addr, const uint8_t *payload, uint8_t len) {
  uint8_t crc = 0;
  crc = crc8Update(crc, cmd);
  crc = crc8Update(crc, addr);
  crc = crc8Update(crc, len);
  for (int i = 0; i < len; i++) crc = crc8Update(crc, payload[i]);
  uint8_t hdr[4] = {PKT_SYNC, cmd, addr, len};
  Serial.write(hdr, 4);
  if (len) Serial.write(payload, len);
  Serial.write(crc);
  Serial.send_now();   // replies are latency-sensitive; don't wait for a full packet
}

void passthroughExit(const char *why) {
  RS485_SERIAL.flush();
  passthrough = false;
  pt_host = FrameParser();
  pt_bus = FrameParser();
  Serial.print("\r\n# passthrough OFF (");
  Serial.print(why);
  Serial.println(")");
}

void passthroughService() {
  while (RS485_SERIAL.available()) {
    if (pt_bus.feed((uint8_t)RS485_SERIAL.read()))
      ptWriteHost(pt_bus.cmd, pt_bus.addr, pt_bus.pay, pt_bus.len);
  }
  // A full RS485_SERIAL TX buffer makes sendPacket() block, which stops this
  // loop reading USB, which back-pressures the host. That is the flow control.
  while (Serial.available()) {
    pt_last_host_ms = millis();
    if (!pt_host.feed((uint8_t)Serial.read())) continue;
    if (pt_host.cmd == 0x00) {
      if (pt_host.addr == 0x00) { passthroughExit("host"); return; }
      if (pt_host.addr == 0x01) ptWriteHost(0x00, 0x01, nullptr, 0);
      continue;
    }
    sendPacket(pt_host.cmd, pt_host.addr, pt_host.pay, pt_host.len);
  }
  if (millis() - pt_last_host_ms > PT_IDLE_MS) passthroughExit("30 s idle");
}

// ── USB vendor interface (docs/USB_PROTOCOL.md → "Command table") ────────────
// The second HID interface (usage page 0xFF00, interface 2): commands from the
// host; responses, events and the telemetry stream back. Every packet is a
// 5-byte header (kind, transaction ID, flags, payload length u16) and then
// the payload. IN packets (to the host) are 64 bytes at both speeds; OUT
// packets (from the host) are 1024 bytes at High Speed and 64 at Full Speed,
// set by the core at enumeration. A message longer than one packet spans
// several, START on the first and END on the last, and byte 0 of a message is
// its code.
//
// It all runs from loop() and never waits on the host. Outgoing packets queue
// here until the core has a free transmit slot; a message that doesn't fit is
// dropped whole and counted. Telemetry is queued only behind nothing else, so
// a slow host loses telemetry, not responses. Events start with the host's
// first command after USB configures, so a pad no program has opened doesn't
// fill up with stale ones.
constexpr uint8_t  VND_PROTOCOL = 2;   // 2: u16 length, OUT size by speed (2026-10-21)
constexpr uint8_t  VND_HEADER = 5;
constexpr uint16_t VND_PACKET = RAWHID_TX_SIZE;     // IN: 64
constexpr uint16_t VND_PAYLOAD = VND_PACKET - VND_HEADER;   // 59
constexpr uint16_t VND_OUT_MAX = RAWHID_RX_SIZE_480;        // OUT at High Speed
constexpr uint8_t  VND_CMD = 1, VND_RESP = 2, VND_EVENT = 3, VND_STREAM = 4;   // kind
constexpr uint8_t  VND_START = 0x01, VND_END = 0x02;                           // flags

constexpr uint8_t  CMD_DEVICE_INFO = 0x01, CMD_GET_CONFIG = 0x02, CMD_SET_CONFIG = 0x03,
                   CMD_PAD_STATUS = 0x04, CMD_STREAM = 0x05, CMD_RESET_CONFIG = 0x06,
                   CMD_RESET_ANIMATIONS = 0x07, CMD_LED_FRAME = 0x08, CMD_UNDERGLOW = 0x09,
                   CMD_LED_MODE = 0x0A, CMD_SELF_TEST = 0x0B, CMD_ZERO_COUNTERS = 0x0C,
                   CMD_PASSTHROUGH = 0x0D, CMD_BUS_FRAME = 0x0E, CMD_REBOOT_BOOTLOADER = 0x0F,
                   CMD_CLOCK_PROBE = 0x10;
// The response to a command that can't be answered in its own format:
// [RESP_ERROR, command code, reason].
constexpr uint8_t  RESP_ERROR = 0x7F;
constexpr uint8_t  ERR_UNKNOWN = 1, ERR_LENGTH = 2, ERR_BUSY = 3, ERR_UNSUPPORTED = 4;
constexpr uint8_t  EVT_PRESENCE = 0x81, EVT_CONFIG_RESULT = 0x82, EVT_CONFIG_RESET = 0x83,
                   EVT_BUS_FRAME = 0x84;
constexpr uint8_t  STREAM_FSR_RAW = 0xC1;
constexpr uint8_t  STREAM_RECORD = 14;   // panel u8, master us u32, then the 'f' payload (9)
constexpr uint8_t  STREAM_MAX_RECORDS = (VND_PAYLOAD - 1) / STREAM_RECORD;   // 4

constexpr uint32_t REBOOT_MAGIC = 0x544F4F42;      // "BOOT" as little-endian bytes
constexpr uint16_t HOST_FRAME_LEN = NUM_PANELS * NUM_LEDS * 3;   // 675
constexpr uint32_t HOST_FRAME_HOLD_MS = 1000;      // then the master's own pattern again
constexpr uint8_t  VND_TXQ = 16;                   // packets

uint8_t  vnd_txq[VND_TXQ][VND_PACKET];
uint8_t  vnd_txq_tail = 0, vnd_txq_count = 0;
uint8_t  vnd_rx_msg[1 + HOST_FRAME_LEN];   // the longest command is an LED frame
uint16_t vnd_rx_len = 0;
uint8_t  vnd_rx_txn = 0;
bool     vnd_rx_open = false;
uint32_t vnd_rx_us = 0;            // when the last packet was read (clock probe)
uint8_t  vnd_usb_config = 0;       // usb_configuration as last seen
bool     vnd_host = false;         // a command has arrived since USB configured
bool     vnd_stream_on = false;
uint8_t  vnd_stream_msg[1 + STREAM_MAX_RECORDS * STREAM_RECORD];
uint8_t  vnd_stream_n = 0;         // records waiting in vnd_stream_msg
bool     vnd_passthrough = false;
uint32_t vnd_pt_last_ms = 0;
// What one USB passthrough session heard, printed when it ends. Added
// 2026-10-21 for `pad_cmd.py bus-h` replies that never arrived: was anything
// heard, and was a packet still open in pt_bus when the next frame went out?
uint16_t vnd_pt_sent = 0, vnd_pt_open_at_send = 0;
uint32_t vnd_pt_bytes = 0, vnd_pt_frames = 0, vnd_pt_crc = 0;
uint8_t  vnd_pt_last[32];          // the last bytes heard, oldest first once full
uint8_t  host_frame[HOST_FRAME_LEN];
uint32_t host_frame_ms = 0;
bool     host_frame_seen = false;
uint32_t vnd_cmds = 0, vnd_bad = 0, vnd_dropped = 0, vnd_stream_dropped = 0;

void vndPump() {
  while (vnd_txq_count && usb_rawhid_try_send(vnd_txq[vnd_txq_tail]) > 0) {
    vnd_txq_tail = (vnd_txq_tail + 1) % VND_TXQ;
    vnd_txq_count--;
  }
}

// Queues one message as packets, all or nothing.
bool vndSend(uint8_t kind, uint8_t txn, const uint8_t *msg, uint16_t len) {
  uint16_t npk = len ? (len + VND_PAYLOAD - 1) / VND_PAYLOAD : 1;
  vndPump();
  if (npk > VND_TXQ - vnd_txq_count) { vnd_dropped++; return false; }
  for (uint16_t k = 0; k < npk; k++) {
    uint8_t *pk = vnd_txq[(vnd_txq_tail + vnd_txq_count) % VND_TXQ];
    uint16_t off = k * VND_PAYLOAD;
    uint16_t n = (len - off) > VND_PAYLOAD ? VND_PAYLOAD : (len - off);
    pk[0] = kind;
    pk[1] = txn;
    pk[2] = (k == 0 ? VND_START : 0) | (k == npk - 1 ? VND_END : 0);
    kp_put_u16(pk + 3, n);
    memcpy(pk + VND_HEADER, msg + off, n);
    memset(pk + VND_HEADER + n, 0, VND_PAYLOAD - n);
    vnd_txq_count++;
  }
  vndPump();
  return true;
}

// Transaction 0 is fire-and-forget: no response, ever.
void vndRespond(uint8_t txn, const uint8_t *msg, uint16_t len) {
  if (txn) vndSend(VND_RESP, txn, msg, len);
}

void vndError(uint8_t txn, uint8_t code, uint8_t reason) {
  uint8_t m[3] = {RESP_ERROR, code, reason};
  vndRespond(txn, m, sizeof(m));
}

void vndEvent(const uint8_t *msg, uint16_t len) {
  if (vnd_host) vndSend(VND_EVENT, 0, msg, len);
}

void vndPresenceEvent(int pidx, bool present) {
  uint8_t m[3] = {EVT_PRESENCE, PANEL_IDS[pidx], (uint8_t)present};
  vndEvent(m, sizeof(m));
}

void vndConfigResultEvent(int pidx) {
  uint8_t m[7] = {EVT_CONFIG_RESULT, PANEL_IDS[pidx], cfg_state[pidx]};
  putU32(m + 3, cfg_panel_crc[pidx]);
  vndEvent(m, sizeof(m));
}

// Telemetry goes out only when nothing else is queued, so at most one stream
// packet waits here; records gather behind it, up to four, and past that the
// newest is dropped. With the queue empty a record leaves at once.
void vndStreamFlush() {
  if (!vnd_stream_n) return;
  vndPump();
  if (vnd_txq_count) return;
  vnd_stream_msg[0] = STREAM_FSR_RAW;
  if (vndSend(VND_STREAM, 0, vnd_stream_msg, 1 + vnd_stream_n * STREAM_RECORD)) vnd_stream_n = 0;
}

// One 'f' reply (`f` = its 9-byte payload, "not configured" bit included).
void vndStreamRecord(uint8_t addr, uint32_t t_us, const uint8_t *f) {
  if (!vnd_stream_on) return;
  if (vnd_stream_n == STREAM_MAX_RECORDS) { vnd_stream_dropped++; return; }
  uint8_t *r = vnd_stream_msg + 1 + vnd_stream_n * STREAM_RECORD;
  r[0] = addr;
  putU32(r + 1, t_us);
  memcpy(r + 5, f, REPLY_F_LEN);
  vnd_stream_n++;
  vndStreamFlush();
}

void vndPassthroughExit(const char *why) {
  RS485_SERIAL.flush();
  vnd_passthrough = false;
  Serial.print("# USB passthrough OFF (");
  Serial.print(why);
  Serial.print("): sent ");
  Serial.print(vnd_pt_sent);
  Serial.print(", ");
  Serial.print(vnd_pt_open_at_send);
  Serial.print(" with a packet open; heard ");
  Serial.print(vnd_pt_bytes);
  Serial.print(" bytes, ");
  Serial.print(vnd_pt_frames);
  Serial.print(" frames, ");
  Serial.print(vnd_pt_crc);
  Serial.print(" CRC fails");
  if (pt_bus.st != FrameParser::W_SYNC) Serial.print(", a packet still open");
  Serial.print(". Last bytes:");
  uint32_t n = vnd_pt_bytes < sizeof(vnd_pt_last) ? vnd_pt_bytes : sizeof(vnd_pt_last);
  for (uint32_t i = vnd_pt_bytes - n; i < vnd_pt_bytes; i++) {
    uint8_t b = vnd_pt_last[i % sizeof(vnd_pt_last)];
    Serial.print(b < 0x10 ? " 0" : " ");
    Serial.print(b, HEX);
  }
  Serial.println();
  pt_bus = FrameParser();
}

// USB configured afresh (plugged in, host reset, reboot): the host side starts
// over, so nothing of the last session carries on.
void vndReset() {
  vnd_txq_tail = vnd_txq_count = 0;
  vnd_rx_open = false;
  vnd_host = vnd_stream_on = false;
  vnd_stream_n = 0;
  host_frame_seen = false;
  if (vnd_passthrough) vndPassthroughExit("USB reset");
}

void vndPadStatus(uint8_t *r) {
  uint32_t now = millis();
  for (int i = 0; i < NUM_PANELS; i++) {
    uint8_t *o = r + i * 12;
    uint32_t age = now - last_reply_ms[i];
    o[0] = panel_online[i];
    kp_put_u16(o + 1, !poll_ok_total[i] ? 0xFFFF : age > 0xFFFE ? 0xFFFE : (uint16_t)age);
    o[3] = poll_pct[i];
    kp_put_u16(o + 4, panel_status[i].got ? panel_status[i].crc_errs : 0);
    o[6] = cfg_state[i];
    putU32(o + 7, panel_status[i].got ? panel_status[i].build_tag : 0);
    o[11] = panel_status[i].got ? panel_status[i].term : 0xFF;
  }
}

void vndCommand(uint8_t txn, const uint8_t *m, uint16_t len) {
  if (!len) { vnd_bad++; return; }
  uint8_t code = m[0];
  const uint8_t *a = m + 1;
  uint16_t n = len - 1;
  if (txn == 0 && code != CMD_LED_FRAME && code != CMD_UNDERGLOW && code != CMD_BUS_FRAME) {
    vnd_bad++;     // only frames may be fire-and-forget
    return;
  }
  vnd_cmds++;
  if (!vnd_host) {
    vnd_host = true;
    if (cfg_was_reset) { uint8_t e = EVT_CONFIG_RESET; vndEvent(&e, 1); }
  }

  static uint8_t r[2 + CFG_BLOCK_LEN];   // the longest response: Get config
  r[0] = code;
  switch (code) {
    case CMD_DEVICE_INFO: {
      bool got[NUM_PANELS], term[NUM_PANELS];
      uint16_t present = 0;
      for (int i = 0; i < NUM_PANELS; i++) {
        got[i] = panel_online[i] && panel_status[i].got;
        term[i] = panel_status[i].term;
        if (panel_online[i]) present |= 1u << PANEL_IDS[i];
      }
      r[1] = VND_PROTOCOL;
      r[2] = usb_high_speed ? 1 : 0;
      kp_put_u16(r + 3, VND_PACKET);
      kp_put_u16(r + 5, usb_high_speed ? RAWHID_RX_SIZE_480 : RAWHID_RX_SIZE_12);
      r[7] = FW_VERSION_MAJOR; r[8] = FW_VERSION_MINOR; r[9] = FW_VERSION_PATCH;
      putU32(r + 10, build_tag);
      r[14] = KP_CONFIG_VERSION;
      r[15] = readPlayerId();
      r[16] = (cfg_was_reset ? 0x01 : 0) | (vnd_stream_on ? 0x02 : 0) |
              (vnd_passthrough ? 0x04 : 0) | (cfg_unsaved ? 0x08 : 0);
      kp_put_u16(r + 17, present);
      r[19] = terminationCode(got, term, &r[20]);
      vndRespond(txn, r, 21);
      break;
    }
    case CMD_GET_CONFIG:
      r[1] = KP_CONFIG_VERSION;
      memcpy(r + 2, cfg_block, CFG_BLOCK_LEN);
      vndRespond(txn, r, 2 + CFG_BLOCK_LEN);
      break;
    case CMD_SET_CONFIG: {
      // Saved first, then pushed; each panel's outcome follows as a config
      // result event. Saving stalls this core: not during gameplay.
      if (n >= 1 && a[0] != KP_CONFIG_VERSION) { r[1] = 1; vndRespond(txn, r, 2); break; }
      if (n != 1 + CFG_BLOCK_LEN) { vndError(txn, code, ERR_LENGTH); break; }
      for (int i = 0; i < NUM_PANELS; i++) {
        int bad = kp_panel_cfg_bad_channel(a + 1 + i * KP_PANEL_CFG_LEN);
        if (bad < 0) continue;
        r[1] = 2; r[2] = PANEL_IDS[i]; r[3] = (uint8_t)bad;
        vndRespond(txn, r, 4);
        return;
      }
      memcpy(cfg_block, a + 1, CFG_BLOCK_LEN);
      r[1] = cfgCommit() ? 0 : 3;
      vndRespond(txn, r, 2);
      Serial.print(r[1] ? "# !! config from USB: SAVE FAILED (read-back mismatch); pushing it anyway"
                        : "# config from USB: saved, pushing to every panel");
      Serial.print(" (save ");
      Serial.print(cfg_save_us);
      Serial.println(" us)");
      break;
    }
    case CMD_PAD_STATUS:
      vndPadStatus(r + 1);
      vndRespond(txn, r, 1 + NUM_PANELS * 12);
      break;
    case CMD_STREAM:
      if (n < 2) { vndError(txn, code, ERR_LENGTH); break; }
      if (a[1] != 0) { r[1] = 1; vndRespond(txn, r, 2); break; }   // raw is the only mode yet
      vnd_stream_on = a[0] != 0;
      vnd_stream_n = 0;
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    case CMD_RESET_CONFIG:
      cfgDefaults();
      r[1] = cfgCommit() ? 0 : 3;
      vndRespond(txn, r, 2);
      Serial.println(r[1] ? "# !! config reset to defaults from USB, but the SAVE FAILED; pushing anyway"
                          : "# config reset to defaults from USB: saved, pushing to every panel");
      break;
    case CMD_LED_FRAME:
      // Newest wins: the master sends whichever frame it holds when its own
      // 60 Hz LED slot comes round.
      if (n != HOST_FRAME_LEN) { vnd_bad++; vndError(txn, code, ERR_LENGTH); break; }
      memcpy(host_frame, a, HOST_FRAME_LEN);
      host_frame_ms = millis();
      host_frame_seen = true;
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    case CMD_LED_MODE: {
      if (n < 4) { vndError(txn, code, ERR_LENGTH); break; }
      uint16_t mask = kp_get_u16(a) & 0x1FF;
      if (!mask || a[2] > 2) { r[1] = 1; vndRespond(txn, r, 2); break; }
      mode_payload[0] = a[2];
      mode_payload[1] = a[3];
      if (mask == 0x1FF) mode_pending = true;
      else               mode_mask_pending |= mask;
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    }
    case CMD_SELF_TEST: {
      if (passthrough || vnd_passthrough) { vndError(txn, code, ERR_BUSY); break; }
      uint8_t slot_id[NUM_PANELS];
      uint16_t acked, low;
      r[1] = identifySelfTest(slot_id, &acked, &low) ? 0 : 1;
      kp_put_u16(r + 2, low);
      kp_put_u16(r + 4, acked);
      for (int i = 0; i < NUM_PANELS; i++) {
        r[6 + 2 * i] = slot_id[i];
        r[7 + 2 * i] = slot_id[i] != 0xFF && ((acked >> slot_id[i]) & 1);
      }
      vndRespond(txn, r, 6 + 2 * NUM_PANELS);
      break;
    }
    case CMD_ZERO_COUNTERS:
      clear_pending = true;
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    case CMD_PASSTHROUGH:
      if (n < 1) { vndError(txn, code, ERR_LENGTH); break; }
      if (a[0] && passthrough) { vndError(txn, code, ERR_BUSY); break; }
      if (a[0] && !vnd_passthrough) {
        waitBusQuiet();
        parseRx();                      // the last reply is ours, not the host's
        pt_bus = FrameParser();
        rx = FrameParser();
        vnd_pt_sent = vnd_pt_open_at_send = 0;
        vnd_pt_bytes = vnd_pt_frames = vnd_pt_crc = 0;
        vnd_passthrough = true;
        vnd_pt_last_ms = millis();
        Serial.println("# USB passthrough ON: the master's own bus traffic stops");
      } else if (!a[0] && vnd_passthrough) {
        vndPassthroughExit("host");
      }
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    case CMD_BUS_FRAME:
      if (!vnd_passthrough) { vnd_bad++; vndError(txn, code, ERR_BUSY); break; }
      if (n < 2 || n - 2 > PKT_MAX_PAYLOAD) { vnd_bad++; vndError(txn, code, ERR_LENGTH); break; }
      vnd_pt_sent++;
      if (pt_bus.st != FrameParser::W_SYNC) vnd_pt_open_at_send++;
      sendPacket(a[0], a[1], a + 2, (uint8_t)(n - 2));
      r[1] = 0;
      vndRespond(txn, r, 2);
      break;
    case CMD_REBOOT_BOOTLOADER: {
      if (n < 4 || getU32(a) != REBOOT_MAGIC) { r[1] = 1; vndRespond(txn, r, 2); break; }
      r[1] = 0;
      vndRespond(txn, r, 2);
      Serial.println("# rebooting into the Teensy bootloader (USB command)");
      // Let the response leave before USB goes away.
      uint32_t t0 = millis();
      while (vnd_txq_count && (millis() - t0) < 100) vndPump();
      delay(50);
      _reboot_Teensyduino_();
      break;
    }
    case CMD_CLOCK_PROBE:
      if (n < 8) { vndError(txn, code, ERR_LENGTH); break; }
      memcpy(r + 1, a, 8);
      putU32(r + 9, vnd_rx_us);
      putU32(r + 13, micros());
      vndRespond(txn, r, 17);
      break;
    case CMD_RESET_ANIMATIONS:   // waits on the animation design
    case CMD_UNDERGLOW:          // deferred (docs/UNDERGLOW.md)
      vndError(txn, code, ERR_UNSUPPORTED);
      break;
    default:
      vnd_bad++;
      vndError(txn, code, ERR_UNKNOWN);
      break;
  }
}

void vndService() {
  if (usb_configuration != vnd_usb_config) {
    vnd_usb_config = usb_configuration;
    vndReset();
  }
  static uint8_t pk[VND_OUT_MAX];
  int size;
  while (usb_rawhid_available() && (size = usb_rawhid_recv(pk, 0)) > 0) {
    vnd_rx_us = micros();
    vnd_pt_last_ms = millis();
    uint8_t flags = pk[2];
    uint16_t n = kp_get_u16(pk + 3);
    if (pk[0] != VND_CMD || n > size - VND_HEADER) { vnd_bad++; continue; }
    if (flags & VND_START) {
      vnd_rx_open = true;
      vnd_rx_txn = pk[1];
      vnd_rx_len = 0;
    } else if (!vnd_rx_open || pk[1] != vnd_rx_txn) {
      vnd_bad++;
      continue;
    }
    if (vnd_rx_len + n > sizeof(vnd_rx_msg)) {
      vnd_bad++;
      vnd_rx_open = false;
      vndError(vnd_rx_txn, vnd_rx_msg[0], ERR_LENGTH);
      continue;
    }
    memcpy(vnd_rx_msg + vnd_rx_len, pk + VND_HEADER, n);
    vnd_rx_len += n;
    if (flags & VND_END) {
      vnd_rx_open = false;
      vndCommand(vnd_rx_txn, vnd_rx_msg, vnd_rx_len);
    }
  }
  vndStreamFlush();
  vndPump();
}

// loop() runs only this while USB passthrough is on, as it runs only
// passthroughService() for the console's `P`: every CRC-valid frame heard on
// the bus goes to the host as a bus frame event, and the host's bus frame
// commands go out unchanged. It ends when the host says so, on USB reset, or
// after PT_IDLE_MS with nothing from the host.
void vndPassthroughService() {
  while (RS485_SERIAL.available()) {
    uint8_t c = (uint8_t)RS485_SERIAL.read();
    vnd_pt_last[vnd_pt_bytes++ % sizeof(vnd_pt_last)] = c;
    if (!pt_bus.feed(c)) {
      if (pt_bus.crc_failed) vnd_pt_crc++;
      continue;
    }
    vnd_pt_frames++;
    uint8_t ev[3 + PKT_MAX_PAYLOAD] = {EVT_BUS_FRAME, pt_bus.cmd, pt_bus.addr};
    memcpy(ev + 3, pt_bus.pay, pt_bus.len);
    vndEvent(ev, 3 + pt_bus.len);
  }
  vndService();
  if (vnd_passthrough && millis() - vnd_pt_last_ms > PT_IDLE_MS) vndPassthroughExit("30 s idle");
}

void vndReport() {
  Serial.print("# USB vendor interface: host ");
  Serial.print(vnd_host ? "active" : "none yet");
  Serial.print("  commands ");
  Serial.print(vnd_cmds);
  Serial.print("  malformed ");
  Serial.print(vnd_bad);
  Serial.print("  dropped ");
  Serial.print(vnd_dropped);
  Serial.print("  stream ");
  Serial.print(vnd_stream_on ? "ON" : "off");
  Serial.print(" (records dropped ");
  Serial.print(vnd_stream_dropped);
  Serial.print(")  passthrough ");
  Serial.print(vnd_passthrough ? "ON" : "off");
  bool host_leds = host_frame_seen && (int32_t)(millis() - host_frame_ms) < (int32_t)HOST_FRAME_HOLD_MS;
  Serial.print("  LEDs from ");
  Serial.println(host_leds ? "the host" : "the master");
}

// ── USB serial commands ──────────────────────────────────────────────────────
// Newline-terminated. The board-local ones (?, n, r, D, u, x) need no panels.
void printHelp() {
  Serial.println(F(
    "commands:\r\n"
    "  ?                          this help\r\n"
    "  n                          INT line states, by panel position\r\n"
    "  N                          INT external pull-up (RN1) check\r\n"
    "  r                          INT GPIO port/bit layout (single-read check)\r\n"
    "  D                          read the player-ID DIP (SW1)\r\n"
    "  u                          underglow colours: red, green, blue, dim white, off\r\n"
    "  g                          underglow chase, one group at a time\r\n"
    "  g <n>                      light underglow group n (1-44) alone and hold\r\n"
    "  x                          pause/resume LED frames + FSR polling\r\n"
    "  I                          slot <-> panel-ID self-test (needs panels)\r\n"
    "  b                          broken-header test: panel parser timeout\r\n"
    "  j                          USB: pad input and vendor interface counters\r\n"
    "  t                          toggle telemetry stream\r\n"
    "  H                          status of every panel (build, uptime, reset, errors)\r\n"
    "  z                          zero bus counters, master and all panels\r\n"
    "  m <mode> [secs]            panel LEDs: 0 normal, 1 ID, 2 sensor test\r\n"
    "  w                          toggle full white on all panels (soak test)\r\n"
    "  P                          bus passthrough for tools/panel_update.py (binary)\r\n"
    "  c                          panel config: the master's copy, each panel's state\r\n"
    "  S <panel> <press> <rel> [ch]  thresholds, all channels or one (0-3); not saved\r\n"
    "  F <panel> <ch> <pop> <en>  channel flags: populated, enabled (0/1); not saved\r\n"
    "  save                       save the config to EEPROM (stalls INT/USB: not in play)\r\n"
    "  reset config               defaults for every panel, saved and pushed"));
}

void handleCommand(const char *s) {
  if (strcmp(s, "?") == 0) {
    printHelp();
  } else if (strcmp(s, "n") == 0) {
    reportIntLines("now");
  } else if (strcmp(s, "N") == 0) {
    reportIntPullups();
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
  } else if (strcmp(s, "g") == 0) {
    underglowChase();
  } else if (s[0] == 'g') {
    int n;
    if (sscanf(s + 1, "%d", &n) == 1 && n >= 1 && n <= UNDERGLOW_GROUPS) {
      underglowSingle(n - 1);
      Serial.print("# underglow group "); Serial.print(n);
      Serial.println(" lit alone. Run `g` or `u` to blank it.");
    } else {
      Serial.println("# usage: g <group 1-44>");
    }
  } else if (strcmp(s, "x") == 0) {
    bus_traffic = !bus_traffic;
    Serial.print("# bus traffic ");
    Serial.println(bus_traffic ? "RESUMED" : "PAUSED");
  } else if (strcmp(s, "w") == 0 || strcmp(s, "W") == 0) {
    white_soak = !white_soak;
    Serial.print("# full white ");
    Serial.print(white_soak ? "ON" : "OFF");
    if (white_soak && !bus_traffic) Serial.print(" (bus traffic is paused: `x` to resume)");
    Serial.println();
  } else if (strcmp(s, "I") == 0) {
    uint8_t slot_id[NUM_PANELS];
    uint16_t acked, low;
    identifySelfTest(slot_id, &acked, &low);
  } else if (strcmp(s, "b") == 0) {
    brokenHeaderTest();
  } else if (strcmp(s, "j") == 0 || strcmp(s, "J") == 0) {
    hidReport();
    vndReport();
  } else if (strcmp(s, "P") == 0) {
    Serial.println("# passthrough ON: binary frames until the host exits or 30 s idle");
    Serial.send_now();
    waitBusQuiet();
    while (RS485_SERIAL.available()) RS485_SERIAL.read();   // nothing stale reaches the host
    pt_host = FrameParser();
    pt_bus = FrameParser();
    rx = FrameParser();                  // and nothing half-parsed waits for after
    pt_last_host_ms = millis();
    passthrough = true;
  } else if (strcmp(s, "H") == 0 || strcmp(s, "h") == 0) {
    if (status_next >= 0) {
      Serial.println("# status sweep already running");
      return;
    }
    for (int i = 0; i < NUM_PANELS; i++) status_got[i] = status_term[i] = false;
    status_next = 0;
    status_verbose = true;
    Serial.println("# status sweep: asking panels 0-8, one per poll slot");
  } else if (strcmp(s, "z") == 0 || strcmp(s, "Z") == 0) {
    clear_pending = true;
  } else if (s[0] == 'm') {
    int mode, secs = 0;
    int n = sscanf(s + 1, "%d %d", &mode, &secs);
    if (n >= 1 && mode >= 0 && mode <= 2 && secs >= 0 && secs <= 255) {
      mode_payload[0] = (uint8_t)mode;
      mode_payload[1] = (uint8_t)secs;
      mode_pending = true;
    } else {
      Serial.println("# usage: m <0 normal | 1 ID | 2 test> [seconds, 0 = until changed]");
    }
  } else if (strcmp(s, "t") == 0 || strcmp(s, "T") == 0) {
    stream_telemetry = !stream_telemetry;
    Serial.print("# telemetry stream ");
    Serial.println(stream_telemetry ? "ON" : "OFF");
  } else if (strcmp(s, "c") == 0 || strcmp(s, "C") == 0) {
    printConfig();
  } else if (strcmp(s, "save") == 0) {
    if (cfgSave()) {
      Serial.print("# config saved: slot ");
      Serial.print(cfg_slot ? 'B' : 'A');
      Serial.print(", sequence ");
      Serial.print(cfg_seq);
      Serial.print(", ");
      Serial.print(cfg_save_us);
      Serial.println(" us");
    } else {
      Serial.println("# !! config save FAILED: EEPROM read-back does not match");
    }
  } else if (strcmp(s, "reset config") == 0) {
    cfgDefaults();
    Serial.println(cfgCommit() ? "# config reset to defaults: saved, pushing to every panel"
                               : "# !! config reset to defaults, but the save FAILED; pushing anyway");
  } else if (s[0] == 'S' || s[0] == 's') {
    // Edits the master's copy; the 'W' goes out in the next poll slot and the
    // panel's 'w' shows up in `c`. Not saved until `save`.
    int panel, press, rel, ch = -1;
    int n = sscanf(s + 1, "%d %d %d %d", &panel, &press, &rel, &ch);
    if (n >= 3 && panel >= 0 && panel < NUM_PANELS &&
        press >= 1 && press <= KP_ADC_MAX && rel >= 0 && rel < press &&
        (n == 3 || (ch >= 0 && ch < KP_NUM_FSR))) {
      for (int c = 0; c < KP_NUM_FSR; c++) {
        if (n == 4 && c != ch) continue;
        uint8_t *chp = cfgPanel(panel) + c * KP_CH_LEN;
        kp_put_u16(chp, (uint16_t)press);
        kp_put_u16(chp + 2, (uint16_t)rel);
      }
      cfg_unsaved = true;
      cfgChanged();
      Serial.print("# panel ");
      Serial.print(PANEL_IDS[panel]);
      Serial.print(n == 4 ? " channel " : " all channels");
      if (n == 4) Serial.print(ch);
      Serial.print(": press ");
      Serial.print(press);
      Serial.print(" release ");
      Serial.print(rel);
      Serial.println(". Sending 'W'; not saved (`save` keeps it)");
    } else {
      Serial.println("# usage: S <panel 0-8> <press 1-4095> <release, below press> [channel 0-3]");
    }
  } else if (s[0] == 'F') {
    int panel, ch, pop, en;
    if (sscanf(s + 1, "%d %d %d %d", &panel, &ch, &pop, &en) == 4 &&
        panel >= 0 && panel < NUM_PANELS && ch >= 0 && ch < KP_NUM_FSR &&
        (pop == 0 || pop == 1) && (en == 0 || en == 1)) {
      cfgPanel(panel)[ch * KP_CH_LEN + 4] = (uint8_t)((pop ? KP_CH_POPULATED : 0) | (en ? KP_CH_ENABLED : 0));
      cfg_unsaved = true;
      cfgChanged();
      Serial.print("# panel ");
      Serial.print(PANEL_IDS[panel]);
      Serial.print(" channel ");
      Serial.print(ch);
      Serial.print(pop ? ": populated" : ": NOT populated");
      Serial.print(en ? ", enabled" : ", disabled");
      Serial.println(". Sending 'W'; not saved (`save` keeps it)");
    } else {
      Serial.println("# usage: F <panel 0-8> <channel 0-3> <populated 0/1> <enabled 0/1>");
    }
  }
}

void pollSerialCommands() {
  static char buf[32];
  static uint8_t len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    // Local echo. The single-letter commands survive without it, but `S <panel>
    // <press> <rel>` is unusable blind — you cannot see a typo before pressing
    // Enter, and a dumb serial terminal does not echo for you. Backspace is
    // handled too, or a correction silently leaves the wrong bytes in `buf`.
    if (c == '\n' || c == '\r') {
      Serial.println();
      if (len > 0) {
        buf[len] = '\0';
        handleCommand(buf);
        len = 0;
        if (passthrough) return;   // whatever follows `P` is binary, not text
      }
    } else if (c == 8 || c == 127) {          // backspace / delete
      if (len > 0) { len--; Serial.print("\b \b"); }
    } else if (c >= 32 && c < 127 && len < sizeof(buf) - 1) {
      buf[len++] = c;
      Serial.write(c);
    }
  }
}

// Called once per heartbeat window. A globally elevated CRC error rate can
// mean many things; a single address with a much worse reply rate than the
// rest is specifically the signature of two boards sharing that panel ID
// (their replies collide on the bus). Resets the window either way.
//
// It judges the window before the one just ended, skipping any panel whose
// presence changed in either: a panel that dies near the end of a window is
// only declared gone PANEL_TIMEOUT_MS later, in the next one. Until 2026-10-21
// it judged the window just ended, and every panel that dropped out or came
// back during play set off this warning.
//
// A rate is capped at 100%: a reply to the last poll of one window is counted
// in the next.
uint32_t poll_sent_prev[NUM_PANELS] = {}, poll_ok_prev[NUM_PANELS] = {};
bool     presence_changed_prev[NUM_PANELS] = {};

uint8_t pollPct(uint32_t ok, uint32_t sent) {
  if (!sent) return 0;
  uint32_t pct = ok * 100 / sent;
  return pct > 100 ? 100 : (uint8_t)pct;
}

void checkPanelHealth() {
  constexpr uint32_t MIN_SAMPLE = 20;   // don't judge on too few polls
  constexpr uint32_t BAD_PCT = 50;
  constexpr uint32_t GOOD_PCT = 90;
  int worst = -1, best = -1;
  uint32_t worst_rate = 101, best_rate = 0;

  // ⚠ A panel at 0% is ABSENT, not a duplicate ID — exclude it entirely.
  // The signature of two boards sharing an address is that both reply SOME of
  // the time, because their replies collide; a panel that never answers is
  // simply not on the bus (or is dead), which is a different fault with a
  // different fix. Including zeros here made a single-panel bench spam a
  // duplicate-ID warning about the eight empty slots on every heartbeat, and
  // it would equally mislabel one genuinely dead panel in a full pad.
  int present = 0;
  for (int i = 0; i < NUM_PANELS; i++) {
    if (presence_changed_prev[i] || presence_changed_window[i]) continue;
    if (poll_sent_prev[i] < MIN_SAMPLE) continue;
    if (poll_ok_prev[i] == 0) continue;            // absent, not colliding
    present++;
    uint32_t rate = pollPct(poll_ok_prev[i], poll_sent_prev[i]);
    if (rate < worst_rate) { worst_rate = rate; worst = i; }
    if (rate > best_rate)  { best_rate = rate; best = i; }
  }

  // With only one panel answering there is nothing to compare it against, so
  // the heuristic cannot say anything either way.
  if (present < 2) { worst = best = -1; }

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
    poll_pct[i] = pollPct(poll_ok_window[i], poll_sent_window[i]);
    poll_sent_prev[i] = poll_sent_window[i];
    poll_ok_prev[i] = poll_ok_window[i];
    presence_changed_prev[i] = presence_changed_window[i];
    poll_sent_window[i] = 0;
    poll_ok_window[i] = 0;
    presence_changed_window[i] = false;
  }
}

// ── Setup / loop ─────────────────────────────────────────────────────────────
// Defined down here, not with build_tag: arduino-cli puts its generated
// prototypes above the first function in the file, and they need Color.
void computeBuildTag() {
  const char *s = __DATE__ " " __TIME__;
  uint32_t h = 2166136261u;
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
  build_tag = h;
}

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

  computeBuildTag();
  char tagbuf[9];
  snprintf(tagbuf, sizeof(tagbuf), "%08lX", (unsigned long)build_tag);
  Serial.print("Master firmware ");
  Serial.print(FW_VERSION_MAJOR);
  Serial.print('.');
  Serial.print(FW_VERSION_MINOR);
  Serial.print('.');
  Serial.print(FW_VERSION_PATCH);
  Serial.print(" (build ");
  Serial.print(tagbuf);
  Serial.println(") — INT capture, RS-485 bus, USB pad input + vendor interface");

  for (int i = 0; i < NUM_PANELS; i++) cfg_state[i] = CFG_ABSENT;
  cfgLoad();
  if (cfg_was_reset) {
    Serial.println("config: NO valid saved config, running on defaults (`c` to see, `save` to keep)");
  } else {
    Serial.print("config: loaded from slot ");
    Serial.print(cfg_slot ? 'B' : 'A');
    Serial.print(", sequence ");
    Serial.println(cfg_seq);
  }
  reportIntLines("at boot");

  for (int i = 0; i < NUM_PANELS; i++)
    attachInterrupt(digitalPinToInterrupt(INT_PINS[i]), INT_ISR[i], CHANGE);
  hidBegin();

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
  static uint32_t next_repush_ms = 0;
  static uint8_t  repush_idx = 0;
  static uint32_t next_bg_status_ms = 0;
  static uint8_t  bg_status_idx = 0;

  if (passthrough) {
    passthroughService();
    vndService();          // commands that need the bus answer "busy" meanwhile
    return;
  }
  if (vnd_passthrough) {
    vndPassthroughService();
    return;
  }

  parseRx();
  vndService();
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

  // Per-panel addressed LED frames at 60Hz: the host's frame if one arrived
  // in the last HOST_FRAME_HOLD_MS; otherwise solid color if that panel is
  // pressed, dim rainbow chase if not. `w` overrides all of them with white.
  uint32_t now_ms = millis();
  updatePresence(now_ms);
  if (bus_traffic && now_ms >= next_frame_ms && replyWindowOver()) {
    next_frame_ms = now_ms + FRAME_INTERVAL_MS;
    bool host_leds = host_frame_seen && (int32_t)(now_ms - host_frame_ms) < (int32_t)HOST_FRAME_HOLD_MS;
    rxResync();
    for (int i = 0; i < NUM_PANELS; i++) {
      uint8_t frame[NUM_LEDS * 3];
      if (white_soak)            buildSolidFrame(frame, Color{255, 255, 255});
      else if (host_leds)        memcpy(frame, host_frame + PANEL_IDS[i] * sizeof(frame), sizeof(frame));
      else if (panel_pressed[i]) buildSolidFrame(frame, PANEL_PRESS_COLOR[i]);
      else                  buildIdleFrame(frame, now_ms);
      sendPacket('L', PANEL_IDS[i], frame, sizeof(frame));
      stat_frames_sent++;
    }
    now_ms = millis();   // the burst blocks ~7ms; never schedule from before it
  }

  // FSR poll on its own clock — telemetry rate is independent of the LED rate.
  // One panel per tick, round-robin: polling all panels back-to-back left no
  // gap for a reply to finish before the next poll went out, risking a real
  // half-duplex bus collision between one panel's reply and the master's next
  // transmission. 5ms >> the ~150us a full poll-reply round trip takes, so
  // round-robining leaves each panel an unambiguous, collision-free window.
  static uint8_t poll_panel_idx = 0;

  // Config re-push: one answering panel every CFG_REPUSH_MS / 9, so each gets
  // its 'W' about once a second. And a background 'H' to one answering panel
  // every STATUS_BG_MS, for USB pad status.
  if (bus_traffic && (int32_t)(now_ms - next_repush_ms) >= 0) {
    next_repush_ms = now_ms + CFG_REPUSH_MS / NUM_PANELS;
    if (panel_online[repush_idx]) cfg_push_due[repush_idx] = true;
    repush_idx = (repush_idx + 1) % NUM_PANELS;
  }
  if (bus_traffic && status_next < 0 && bg_status_due < 0 &&
      (int32_t)(now_ms - next_bg_status_ms) >= 0) {
    next_bg_status_ms = now_ms + STATUS_BG_MS;
    for (int k = 0; k < NUM_PANELS; k++) {
      int i = (bg_status_idx + k) % NUM_PANELS;
      if (!panel_online[i]) continue;
      bg_status_due = i;
      bg_status_idx = (uint8_t)((i + 1) % NUM_PANELS);
      break;
    }
  }

  // Queued commands, config pushes and the status sweep take a poll slot each
  // instead of the poll, and the round-robin resumes after. They are sent even
  // with bus traffic paused, since then nothing can collide.
  bool poll_due = replyWindowOver() && (!bus_traffic || now_ms >= next_poll_ms);
  int push = poll_due ? cfgNextPush() : -1;
  if (clear_pending && poll_due) {
    sendRequest('Z', BROADCAST, nullptr, 0, 0);   // broadcast: no reply
    stat_replies = stat_crc_errs = stat_polls_sent = stat_frames_sent = stat_rx_dropped = 0;
    for (int i = 0; i < NUM_PANELS; i++) poll_sent_window[i] = poll_ok_window[i] = 0;
    next_poll_ms = millis() + POLL_INTERVAL_MS;
    clear_pending = false;
    Serial.println("# counters zeroed: master and every panel ('Z' broadcast)");
  } else if (mode_pending && poll_due) {
    sendRequest('M', BROADCAST, mode_payload, sizeof(mode_payload), 0);   // no reply
    next_poll_ms = millis() + POLL_INTERVAL_MS;
    mode_pending = false;
    static const char *const MODE_NAMES[] = {"normal", "ID", "sensor test"};
    Serial.print("# panel LED mode -> ");
    Serial.print(MODE_NAMES[mode_payload[0]]);
    if (mode_payload[1]) { Serial.print(" for "); Serial.print(mode_payload[1]); Serial.print(" s"); }
    Serial.println(" ('M' broadcast; the bring-up build ignores it)");
  } else if (mode_mask_pending && poll_due) {
    for (int i = 0; i < NUM_PANELS; i++) {
      if (!(mode_mask_pending & (1u << PANEL_IDS[i]))) continue;
      mode_mask_pending &= ~(1u << PANEL_IDS[i]);
      sendRequest('M', PANEL_IDS[i], mode_payload, sizeof(mode_payload), REPLY_M_LEN);
      break;
    }
    next_poll_ms = millis() + POLL_INTERVAL_MS;
  } else if (push >= 0) {
    cfgSendPush(push);
    next_poll_ms = millis() + POLL_INTERVAL_MS;
  } else if (status_next >= 0 && poll_due) {
    // The previous panel's reply window has closed by now (poll_due waits for
    // it), so silence from it is final, once whatever arrived since the top of
    // loop() has been parsed.
    parseRx();
    if (status_next > 0 && !status_got[status_next - 1]) {
      Serial.print("# status ");
      Serial.print(PANEL_NAME[status_next - 1]);
      Serial.print("\t id ");
      Serial.print(PANEL_IDS[status_next - 1]);
      Serial.println("  NO REPLY -- absent, unpowered, wrong DIP ID, or old firmware without 'H'");
    }
    if (status_next < NUM_PANELS) {
      sendRequest('H', PANEL_IDS[status_next], nullptr, 0, REPLY_H_LEN);
      status_next++;
    } else {
      int n = 0;
      for (int i = 0; i < NUM_PANELS; i++) n += status_got[i];
      Serial.print("# status sweep done: ");
      Serial.print(n);
      Serial.println("/9 panels answered");
      checkTermination();
      status_next = -1;
      status_verbose = false;
    }
    next_poll_ms = millis() + POLL_INTERVAL_MS;
  } else if (bg_status_due >= 0 && poll_due) {
    sendRequest('H', PANEL_IDS[bg_status_due], nullptr, 0, REPLY_H_LEN);
    bg_status_due = -1;
    next_poll_ms = millis() + POLL_INTERVAL_MS;
  } else if (bus_traffic && poll_due) {
    sendRequest('F', PANEL_IDS[poll_panel_idx], nullptr, 0, REPLY_F_LEN);
    next_poll_ms = millis() + POLL_INTERVAL_MS;
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
      Serial.print(panel_pressed_mask[i]);
      // Last field: 1 if the panel replied within PANEL_TIMEOUT_MS, so a reader
      // can tell live values from the last ones a vanished panel sent.
      bool live = poll_ok_total[i] && (now_ms - last_reply_ms[i]) < PANEL_TIMEOUT_MS;
      Serial.print(' ');
      Serial.println(live ? 1 : 0);
    }
  }

  if (now_ms >= next_heartbeat_ms) {
    next_heartbeat_ms += 5000;
    checkPanelHealth();
    // Nine panels would make the old single-line heartbeat unreadable, and with
    // no panels attached it was nine lines of zeros. Report only what replied.
    // Presence changes are printed as they happen (panelSeen, updatePresence).

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
        // Never print a stale reading as if it were current.
        if (!panel_online[i]) {
          Serial.print("  NO REPLY for ");
          Serial.print((now_ms - last_reply_ms[i]) / 1000.0f, 1);
          Serial.println("s  (values below would be stale, withheld)");
          continue;
        }
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
    if (stat_rx_dropped) {
      Serial.print(", ");
      Serial.print(stat_rx_dropped);
      Serial.print(" partial packets dropped");
    }
    Serial.print(" | config: ");
    int online = 0, match = 0;
    for (int i = 0; i < NUM_PANELS; i++) {
      online += panel_online[i];
      match += panel_online[i] && cfg_state[i] == CFG_MATCH;
    }
    Serial.print(match);
    Serial.print("/");
    Serial.print(online);
    Serial.print(" answering panels match");
    if (cfg_unsaved) Serial.print(", UNSAVED edits");
    if (cfg_was_reset) Serial.print(", on defaults (nothing saved)");
    if (dropped_edges) {
      Serial.print(", dropped edges: ");
      Serial.print(dropped_edges);
    }
    Serial.println();
  }
}
