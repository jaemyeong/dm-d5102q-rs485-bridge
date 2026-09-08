#pragma once
#include <Arduino.h>

// C008 ATOM Lite front button: GPIO39, active LOW, board external pull-up.
// GPIO39 is input-only and has no internal pull-up; never drive this pin.
// BOOT-RESET-MDNS-20260909 is the sole exception to B0's no-GPIO boundary.
inline void prepareBootButton() { pinMode(39, INPUT); }
inline bool bootButtonPressed() { return digitalRead(39) == LOW; }
