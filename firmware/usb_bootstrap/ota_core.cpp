#include "ota_core.h"
#include "vendor/monocypher/monocypher-ed25519.h"
#include <string.h>

namespace bootstrap { namespace ota {
namespace {
constexpr uint8_t magic[8] = {'D','M','O','T','A','1','\r','\n'};
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put32(uint8_t* p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(value >> (i * 8));
}
bool textField(const uint8_t* bytes, size_t size) {
  size_t i = 0;
  for (; i < size && bytes[i]; ++i) {
    const uint8_t c = bytes[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
  }
  if (!i || i == size) return false;
  for (; i < size; ++i) if (bytes[i]) return false;
  return true;
}
}
bool decodeManifest(const uint8_t* bytes, size_t size, Manifest& result) {
  result = Manifest{};
  if (!bytes || size != kManifestBytes || memcmp(bytes, magic, 8) ||
      !textField(bytes + 20, 16) || !textField(bytes + 36, 48)) return false;
  result.version = u32(bytes + 8); result.imageSize = u32(bytes + 12);
  result.minSchema = u32(bytes + 16);
  memcpy(result.boardId, bytes + 20, 16); memcpy(result.buildId, bytes + 36, 48);
  memcpy(result.sha256, bytes + 84, 32);
  return result.version && result.imageSize && result.imageSize <= kImageMax && result.minSchema;
}
void encodeManifest(const Manifest& m, uint8_t bytes[kManifestBytes]) {
  memset(bytes, 0, kManifestBytes); memcpy(bytes, magic, 8);
  put32(bytes + 8, m.version); put32(bytes + 12, m.imageSize); put32(bytes + 16, m.minSchema);
  memcpy(bytes + 20, m.boardId, 16); memcpy(bytes + 36, m.buildId, 48);
  memcpy(bytes + 84, m.sha256, 32);
}
bool validToken(const char* token) {
  if (!token || strlen(token) != 32) return false;
  for (size_t i = 0; i < 32; ++i)
    if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f'))) return false;
  return true;
}
bool usablePublicKey(const uint8_t key[32]) {
  if (!key) return false;
  uint8_t any = 0;
  for (unsigned i = 0; i < 32; ++i) any |= key[i];
  return any != 0; // Unprovisioned builds fail closed; never ship a fixture key.
}
bool Updater::fail(const char* reason) {
  if (phase_ == Phase::Receiving) writer_.abort();
  phase_ = Phase::Failed; reason_ = reason;
  memset(token_, 0, sizeof(token_));
  return false;
}
bool Updater::prepare(const uint8_t* bytes, size_t size, const char* token, uint32_t now) {
  tick(now);
  if (phase_ != Phase::Idle && phase_ != Phase::Failed) { reason_ = "OTA_BUSY"; return false; }
  if (!enabled()) return fail("OTA_KEY_MISSING");
  Manifest candidate;
  if (!bytes || size != kEnvelopeBytes || !decodeManifest(bytes, kManifestBytes, candidate))
    return fail("MANIFEST_INVALID");
  if (crypto_ed25519_check(bytes + kManifestBytes, key_, bytes, kManifestBytes))
    return fail("SIGNATURE_INVALID");
  if (strcmp(candidate.boardId, kBoardId)) return fail("BOARD_MISMATCH");
  if (candidate.minSchema > schema_) return fail("SCHEMA_MISMATCH");
  if (candidate.version <= version_) return fail("DOWNGRADE_REJECTED");
  if (!validToken(token)) return fail("TOKEN_INVALID");
  manifest_ = candidate; memcpy(token_, token, sizeof(token_));
  received_ = 0; started_ = progress_ = now; phase_ = Phase::Prepared; reason_ = "NONE";
  return true;
}
bool Updater::start(const char* token, size_t size, uint32_t now) {
  tick(now);
  if (phase_ != Phase::Prepared) { reason_ = "NOT_PREPARED"; return false; }
  if (!validToken(token) || !equalSecret(token_, token)) { reason_ = "TOKEN_REJECTED"; return false; }
  if (size != manifest_.imageSize) return fail("IMAGE_SIZE_MISMATCH");
  // Consumed before the first flash side effect; a second start never reopens it.
  phase_ = Phase::Receiving; started_ = progress_ = now;
  if (!writer_.begin(manifest_.imageSize)) return fail("OTA_BEGIN_FAILED");
  return true;
}
bool Updater::chunk(const uint8_t* bytes, size_t size, uint32_t now) {
  tick(now);
  if (phase_ != Phase::Receiving) return false;
  if (!bytes || !size || size > kChunkMax || size > manifest_.imageSize - received_)
    return fail("IMAGE_BOUNDS");
  if (!writer_.write(bytes, size)) return fail("OTA_WRITE_FAILED");
  received_ += static_cast<uint32_t>(size); progress_ = now;
  if (received_ != manifest_.imageSize) return true;
  uint8_t digest[32];
  if (!writer_.finish(digest)) return fail("IMAGE_INVALID");
  if (crypto_verify32(digest, manifest_.sha256)) return fail("IMAGE_HASH_MISMATCH");
  if (!writer_.select(manifest_, token_)) return fail("BOOT_SELECT_FAILED");
  phase_ = Phase::RebootPending; reason_ = "NONE";
  return true;
}
void Updater::tick(uint32_t now) {
  if (phase_ == Phase::Prepared && elapsed(now, started_, kPrepareMs)) fail("TOKEN_EXPIRED");
  else if (phase_ == Phase::Receiving &&
           (elapsed(now, started_, kTransferMs) || elapsed(now, progress_, kIdleMs))) fail("UPLOAD_TIMEOUT");
}
void Updater::interrupt() { if (phase_ == Phase::Receiving) fail("UPLOAD_INTERRUPTED"); }
void Updater::resetForBoot() {
  interrupt(); phase_ = Phase::Idle; reason_ = "NONE"; manifest_ = Manifest{};
  started_ = progress_ = received_ = 0; memset(token_, 0, sizeof(token_));
}
const char* phaseName(Phase phase) {
  switch (phase) {
    case Phase::Idle: return "IDLE";
    case Phase::Prepared: return "PREPARED";
    case Phase::Receiving: return "RECEIVING";
    case Phase::RebootPending: return "REBOOT_PENDING";
    default: return "FAILED";
  }
}
}}
