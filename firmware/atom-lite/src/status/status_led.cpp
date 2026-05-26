#include "status_led.h"

namespace dm {

void StatusLed::begin() {
  enabled_ = STATUS_LED_PIN >= 0 && STATUS_LED_COUNT > 0;
  if (!enabled_) return;

  pixels_.begin();
  pixels_.setBrightness(STATUS_LED_BRIGHTNESS);
  off();
}

void StatusLed::setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  if (!enabled_) return;

  const uint32_t color = pixels_.Color(red, green, blue);
  if (color == lastColor_) return;

  for (uint16_t i = 0; i < STATUS_LED_COUNT; ++i) {
    pixels_.setPixelColor(i, color);
  }
  pixels_.show();
  lastColor_ = color;
}

void StatusLed::off() {
  setRgb(0, 0, 0);
}

}  // namespace dm
