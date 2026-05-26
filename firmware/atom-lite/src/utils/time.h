#pragma once

#include <Arduino.h>

namespace dm {

String uptimeString(uint32_t seconds);
String timestampString(uint32_t millisValue = millis());

}  // namespace dm
