#pragma once
#include "ota_core.h"
#include <string.h>

namespace bootstrap { namespace ota {
// Sidecar only. Never change the old dmboot/ota 152-byte journal: the previous
// image must still parse it if the bootloader rolls back across the migration.
constexpr size_t kContextBytes = 164;
constexpr char kContextKey[] = "otactx2";
struct Context {
  Manifest manifest;
  char transaction[33] = {};
  Origin origin = Origin::LegacyPush;
};
enum class ContextRead { Missing, Valid, Invalid };
inline void encodeContext(const Context& context, uint8_t bytes[kContextBytes]) {
  memset(bytes, 0, kContextBytes); memcpy(bytes, "DMCTX2\r\n", 8);
  encodeManifest(context.manifest, bytes + 8);
  memcpy(bytes + 124, context.transaction, 32);
  bytes[156] = static_cast<uint8_t>(context.origin);
  const uint32_t crc = crc32(bytes, kContextBytes - 4);
  for (unsigned i = 0; i < 4; ++i) bytes[160 + i] = static_cast<uint8_t>(crc >> (8 * i));
}
inline ContextRead readContext(Storage& storage, Context& context) {
  context = Context{};
  uint8_t bytes[kContextBytes];
  const size_t size = storage.read(kContextKey, bytes, sizeof(bytes));
  if (!size) return ContextRead::Missing;
  if (size != sizeof(bytes) || memcmp(bytes, "DMCTX2\r\n", 8) || bytes[157] || bytes[158] || bytes[159] ||
      (bytes[156] != static_cast<uint8_t>(Origin::WebFile) && bytes[156] != static_cast<uint8_t>(Origin::GithubPull)))
    return ContextRead::Invalid;
  uint32_t crc = 0;
  for (unsigned i = 0; i < 4; ++i) crc |= uint32_t(bytes[160 + i]) << (8 * i);
  if (crc != crc32(bytes, 160) || !decodeManifest(bytes + 8, kManifestBytes, context.manifest))
    return ContextRead::Invalid;
  memcpy(context.transaction, bytes + 124, 32);
  context.origin = static_cast<Origin>(bytes[156]);
  return validToken(context.transaction) ? ContextRead::Valid : ContextRead::Invalid;
}
inline bool contextMatches(const Context& context, const Manifest& manifest, const char* transaction) {
  if (!validToken(transaction) || !equalSecret(context.transaction, transaction)) return false;
  uint8_t expected[kManifestBytes], actual[kManifestBytes];
  encodeManifest(context.manifest, expected); encodeManifest(manifest, actual);
  return memcmp(expected, actual, sizeof(expected)) == 0;
}
inline bool writeContext(Storage& storage, const Context& context) {
  uint8_t bytes[kContextBytes], check[kContextBytes];
  encodeContext(context, bytes);
  return storage.write(kContextKey, bytes, sizeof(bytes)) &&
    storage.read(kContextKey, check, sizeof(check)) == sizeof(check) && !memcmp(bytes, check, sizeof(bytes));
}
}}
