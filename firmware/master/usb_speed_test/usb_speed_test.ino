// USB High Speed verification test — Teensy 4.0
//
// Question: does Teensyduino actually negotiate USB 2.0 High Speed (480 Mbit/s)
// with this host over this cable, or does it fall back to Full Speed (12 Mbit/s)?
// CLAUDE.md's USB note assumed HS mode needs custom descriptor/stack work; the
// Teensyduino core (usb.c) sets a `usb_high_speed` flag automatically during
// enumeration, which suggests otherwise. This sketch measures real throughput
// to settle it empirically rather than trust either claim blind.
//
// Protocol: host sends 'G'+'\n' (go). Teensy floods raw bytes for TEST_MS
// as fast as USB allows, then sends a summary line the host cross-checks
// against its own wall-clock byte count.
//
// Flash: arduino-cli compile --fqbn teensy:avr:teensy40 firmware/master/usb_speed_test
//        arduino-cli upload  --fqbn teensy:avr:teensy40 firmware/master/usb_speed_test

extern volatile uint8_t usb_high_speed;  // set by Teensyduino core during enumeration

constexpr uint32_t TEST_MS = 3000;
constexpr size_t CHUNK = 512;
uint8_t chunk_buf[CHUNK];

void setup() {
  for (size_t i = 0; i < CHUNK; i++) chunk_buf[i] = (uint8_t)i;  // deterministic pattern

  Serial.begin(0);  // baud ignored on Teensy USB serial
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 5000) {}

  Serial.println("USB speed test ready");
  Serial.print("usb_high_speed flag: ");
  Serial.println(usb_high_speed);
  Serial.println(usb_high_speed
    ? "-> negotiated HIGH SPEED (480 Mbit/s)"
    : "-> negotiated FULL SPEED (12 Mbit/s) or not yet enumerated");
  Serial.println("Send 'G' to start a throughput burst.");
}

void loop() {
  if (Serial.available() && Serial.read() == 'G') {
    Serial.print("usb_high_speed at burst start: ");
    Serial.println(usb_high_speed);
    Serial.println("GO");
    delay(50);  // let the host settle into read mode

    uint32_t start_us = micros();
    uint32_t end_ms = millis() + TEST_MS;
    uint64_t bytes_sent = 0;
    while (millis() < end_ms) {
      size_t n = Serial.write(chunk_buf, CHUNK);
      bytes_sent += n;
    }
    Serial.flush();
    uint32_t elapsed_us = micros() - start_us;

    // Small delay so the summary line doesn't race the last data chunk through
    // the host's read buffer.
    delay(100);
    Serial.print("DONE bytes=");
    Serial.print((uint32_t)bytes_sent);
    Serial.print(" elapsed_us=");
    Serial.print(elapsed_us);
    Serial.print(" mbps=");
    Serial.println((bytes_sent * 8.0) / elapsed_us, 2);
  }
}
