#include "time.h"

namespace dm {

namespace {

String two(uint32_t value) {
  return value < 10 ? "0" + String(value) : String(value);
}

}  // namespace

String uptimeString(uint32_t seconds) {
  uint32_t days = seconds / 86400UL;
  uint32_t h = (seconds % 86400UL) / 3600UL;
  uint32_t m = (seconds % 3600UL) / 60UL;
  uint32_t s = seconds % 60UL;
  String out;
  if (days > 0) {
    out += String(days);
    out += "d ";
  }
  out += two(h);
  out += ':';
  out += two(m);
  out += ':';
  out += two(s);
  return out;
}

String timestampString(uint32_t millisValue) {
  uint32_t totalSeconds = millisValue / 1000UL;
  uint32_t ms = millisValue % 1000UL;
  uint32_t h = (totalSeconds / 3600UL) % 24UL;
  uint32_t m = (totalSeconds / 60UL) % 60UL;
  uint32_t s = totalSeconds % 60UL;
  String out = "[";
  out += two(h);
  out += ':';
  out += two(m);
  out += ':';
  out += two(s);
  out += '.';
  if (ms < 100) out += '0';
  if (ms < 10) out += '0';
  out += String(ms);
  out += ']';
  return out;
}

}  // namespace dm
