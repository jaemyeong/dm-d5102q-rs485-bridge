#pragma once

#include <Arduino.h>

namespace dm {

constexpr size_t kMaxPacketBytes = 256;

struct HexParseResult {
  bool ok = false;
  String error;
  uint8_t bytes[kMaxPacketBytes] = {0};
  size_t length = 0;
};

HexParseResult parseHex(const String& input, size_t maxBytes = kMaxPacketBytes);
String bytesToHex(const uint8_t* data, size_t length);
bool isHexText(const String& input);

}  // namespace dm
