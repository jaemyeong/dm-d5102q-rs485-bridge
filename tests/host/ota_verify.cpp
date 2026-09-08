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
  uint8_t key[32], envelope[kPackageHeaderBytes + 1];
  if (fread(key, 1, sizeof(key), stdin) != sizeof(key)) return 2;
  const size_t size = fread(envelope, 1, sizeof(envelope), stdin);
  if (size != kEnvelopeBytes && size != kPackageHeaderBytes) return 2;
  NoWrites writer; Updater updater(writer, key, 200, 1);
  const char* token = "0123456789abcdef0123456789abcdef";
  return (size == kEnvelopeBytes ? updater.prepare(envelope, size, token, 0) :
    updater.preparePackage(envelope, size, token, Origin::WebFile, 0)) ? 0 : 1;
}
