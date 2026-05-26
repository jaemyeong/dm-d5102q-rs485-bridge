#include "hex.h"

namespace dm {

namespace {

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool isSeparator(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ':' || c == '-' || c == '_';
}

}  // namespace

bool isHexText(const String& input) {
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (isSeparator(c)) continue;
    if (hexValue(c) < 0) return false;
  }
  return true;
}

HexParseResult parseHex(const String& input, size_t maxBytes) {
  HexParseResult result;
  String compact;
  compact.reserve(input.length());

  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (isSeparator(c)) continue;
    if (hexValue(c) < 0) {
      result.error = "bad_hex";
      return result;
    }
    compact += c;
  }

  if (compact.length() == 0) {
    result.error = "empty_hex";
    return result;
  }
  if (compact.length() % 2 != 0) {
    result.error = "odd_length";
    return result;
  }
  if (compact.length() / 2 > maxBytes || compact.length() / 2 > kMaxPacketBytes) {
    result.error = "too_long";
    return result;
  }

  for (size_t i = 0; i < compact.length(); i += 2) {
    int hi = hexValue(compact.charAt(i));
    int lo = hexValue(compact.charAt(i + 1));
    result.bytes[result.length++] = static_cast<uint8_t>((hi << 4) | lo);
  }

  result.ok = true;
  return result;
}

String bytesToHex(const uint8_t* data, size_t length) {
  static const char* digits = "0123456789ABCDEF";
  String out;
  if (!data || length == 0) return out;
  out.reserve(length * 3);
  for (size_t i = 0; i < length; ++i) {
    if (i) out += ' ';
    out += digits[(data[i] >> 4) & 0x0F];
    out += digits[data[i] & 0x0F];
  }
  return out;
}

}  // namespace dm
