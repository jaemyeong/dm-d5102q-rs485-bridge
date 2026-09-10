#pragma once
#include "core.h"
#include <string.h>

namespace bootstrap {
// Versioned exact records: absent/corrupt/unreadable is OFF. No periodic writes.
// A failed commit/readback has an UNKNOWN next-boot outcome, never success.
class AutomaticPolicy {
 public:
  explicit AutomaticPolicy(Storage& storage) : storage_(storage) {}
  bool load() {
    enabled_ = false;
    uint8_t bytes[8] = {};
    const size_t size = storage_.read("ghauto", bytes, sizeof(bytes));
    healthy_ = size == 0;
    for (unsigned value = 0; size == sizeof(bytes) && value < 2; ++value) {
      uint8_t expected[8]; encode(value != 0, expected);
      if (!memcmp(bytes, expected, sizeof(bytes))) {
        healthy_ = true; enabled_ = value != 0; break;
      }
    }
    return healthy_;
  }
  bool save(bool enabled) {
    if (healthy_ && enabled_ == enabled) return true;
    uint8_t bytes[8], verify[8] = {};
    encode(enabled, bytes);
    healthy_ = false; enabled_ = false;
    if (!storage_.write("ghauto", bytes, sizeof(bytes)) ||
        storage_.read("ghauto", verify, sizeof(verify)) != sizeof(verify) ||
        memcmp(bytes, verify, sizeof(bytes))) return false;
    healthy_ = true; enabled_ = enabled; return true;
  }
  bool enabled() const { return enabled_; }
  bool healthy() const { return healthy_; }
 private:
  static void encode(bool enabled, uint8_t out[8]) {
    const uint8_t bytes[] = {'G', 'H', 'A', 1, uint8_t(enabled),
      uint8_t(enabled ? 0xfe : 0xff), 0x6d, 0xa3};
    memcpy(out, bytes, sizeof(bytes));
  }
  Storage& storage_;
  bool enabled_ = false, healthy_ = false;
};
}
