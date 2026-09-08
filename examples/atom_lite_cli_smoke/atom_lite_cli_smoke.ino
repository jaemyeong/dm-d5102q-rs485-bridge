/*
 * ATOM Lite Arduino CLI smoke test
 *
 * Purpose:
 * - Prove that the selected M5Atom board package compiles.
 * - Prove that Arduino CLI can upload to the connected ATOM Lite.
 * - Emit a structured heartbeat for serial-runtime verification.
 *
 * Hardware:
 * - M5Stack ATOM Lite connected over USB-C.
 * - No external wiring is used; no GPIO is configured.
 *
 * Serial monitor: 115200 baud
 * Expected output:
 * t_ms=... level=INFO event=boot target=atom-lite test=arduino-cli-smoke
 * t_ms=... level=INFO event=heartbeat target=atom-lite count=1
 */

#include <Arduino.h>

#if !defined(ARDUINO_M5STACK_ATOM) && !defined(ARDUINO_M5Stack_ATOM)
#error "This smoke test must be compiled for the M5Atom board target."
#endif

static constexpr unsigned long HEARTBEAT_INTERVAL_MS = 1000UL;

static unsigned long previous_heartbeat_ms = 0UL;
static unsigned long heartbeat_count = 0UL;

static void printHeartbeat(const unsigned long now_ms) {
  ++heartbeat_count;
  Serial.print(F("t_ms="));
  Serial.print(now_ms);
  Serial.print(F(" level=INFO event=heartbeat target=atom-lite count="));
  Serial.println(heartbeat_count);
}

void setup() {
  Serial.begin(115200);
  previous_heartbeat_ms = millis();

  Serial.print(F("t_ms="));
  Serial.print(previous_heartbeat_ms);
  Serial.println(F(" level=INFO event=boot target=atom-lite test=arduino-cli-smoke"));
}

void loop() {
  const unsigned long now_ms = millis();
  if (static_cast<unsigned long>(now_ms - previous_heartbeat_ms) >=
      HEARTBEAT_INTERVAL_MS) {
    previous_heartbeat_ms = now_ms;
    printHeartbeat(now_ms);
  }
}
