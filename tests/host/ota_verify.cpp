#include "ota_core.h"
#include <stdio.h>
using namespace bootstrap::ota;
struct NoWrites : ImageWriter {
  bool begin(uint32_t) override { __builtin_trap(); }
  bool write(const uint8_t*, size_t) override { __builtin_trap(); }
  bool finish(uint8_t*) override { __builtin_trap(); }
  bool select(const Manifest&, const char*) override { __builtin_trap(); }
  void abort() override { __builtin_trap(); }
};
int main() {
  uint8_t key[32], envelope[kEnvelopeBytes];
  if (fread(key, 1, sizeof(key), stdin) != sizeof(key) ||
      fread(envelope, 1, sizeof(envelope), stdin) != sizeof(envelope) || getchar() != EOF) return 2;
  NoWrites writer; Updater updater(writer, key, 200, 1);
  return updater.prepare(envelope, sizeof(envelope), "0123456789abcdef0123456789abcdef", 0) ? 0 : 1;
}
