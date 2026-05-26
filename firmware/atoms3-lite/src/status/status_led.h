#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "board_config.h"

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

#ifndef STATUS_LED_COUNT
#define STATUS_LED_COUNT 1
#endif

#ifndef STATUS_LED_BRIGHTNESS
#define STATUS_LED_BRIGHTNESS 32
#endif

namespace dm {

class StatusLed {
 public:
  void begin();
  void setRgb(uint8_t red, uint8_t green, uint8_t blue);
  void off();

 private:
  Adafruit_NeoPixel pixels_{STATUS_LED_COUNT, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800};
  uint32_t lastColor_ = 0xffffffffUL;
  bool enabled_ = false;
};

}  // namespace dm
