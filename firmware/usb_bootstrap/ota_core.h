#pragma once
#include "core.h"

namespace bootstrap { namespace ota {
constexpr size_t kManifestBytes = 116;
constexpr size_t kEnvelopeBytes = kManifestBytes + 64;
constexpr size_t kPackageManifestBytes = 128;
constexpr size_t kPackageHeaderBytes = kPackageManifestBytes + 64;
constexpr uint32_t kUpdaterProtocol = 2;
constexpr uint32_t kImageMax = 1310720;
constexpr uint32_t kPrepareMs = 30000;
constexpr uint32_t kTransferMs = 120000;
constexpr uint32_t kIdleMs = 8000;
constexpr uint32_t kHealthMs = 30000;
constexpr uint32_t kLocalHealthMs = 5000;
constexpr uint32_t kHealthPollMaxMs = 250;
constexpr size_t kChunkMax = 1024;
constexpr char kBoardId[] = "m5stack-atom";

struct Manifest {
  uint32_t version = 0, imageSize = 0, minSchema = 0;
  char boardId[16] = {}, buildId[48] = {};
  uint8_t sha256[32] = {};
  uint32_t minUpdater = 1;
  char channel[8] = {};
};
enum class Origin : uint8_t { LegacyPush = 0, WebFile = 1, GithubPull = 2 };
const char* originName(Origin origin);
bool decodeManifest(const uint8_t* bytes, size_t size, Manifest& result);
void encodeManifest(const Manifest& manifest, uint8_t bytes[kManifestBytes]);
bool decodePackageManifest(const uint8_t* bytes, size_t size, Manifest& result);
void encodePackageManifest(const Manifest& manifest, uint8_t bytes[kPackageManifestBytes]);
bool validToken(const char* token);
bool usablePublicKey(const uint8_t key[32]);

// Adapters may touch only the inactive app slot. Selecting boot is a distinct
// operation, reached only after signature, bounds, image and hash checks.
struct ImageWriter {
  virtual ~ImageWriter() = default;
  // Called only by the coordinator after signature/policy and before flash.
  virtual bool available(const Manifest&) { return true; }
  virtual bool authorize(const Manifest&, const char*, Origin) { return true; }
  virtual bool begin(uint32_t size) = 0;
  virtual bool write(const uint8_t* bytes, size_t size) = 0;
  virtual bool finish(uint8_t sha256[32]) = 0;
  virtual bool select(const Manifest& manifest, const char* transaction) = 0;
  virtual void abort() = 0;
};
enum class Phase { Idle, Prepared, Receiving, RebootPending, Failed };
class Updater {
 public:
  Updater(ImageWriter& writer, const uint8_t* key, uint32_t version, uint32_t schema)
    : writer_(writer), key_(key), version_(version), schema_(schema) {}
  bool prepare(const uint8_t* envelope, size_t size, const char* token, uint32_t now);
  bool preparePackage(const uint8_t* header, size_t size, const char* token, Origin origin, uint32_t now);
  bool start(const char* token, size_t size, uint32_t now);
  bool chunk(const uint8_t* bytes, size_t size, uint32_t now);
  void tick(uint32_t now);
  void interrupt();
  void resetForBoot();
  Phase phase() const { return phase_; }
  const char* reason() const { return reason_; }
  const char* transaction() const { return token_; }
  const Manifest& manifest() const { return manifest_; }
  Origin origin() const { return origin_; }
  uint32_t received() const { return received_; }
  bool enabled() const { return usablePublicKey(key_); }
 private:
  bool fail(const char* reason);
  bool prepareAs(const uint8_t* bytes, size_t size, const char* token, Origin origin, uint32_t now);
  ImageWriter& writer_;
  const uint8_t* key_;
  uint32_t version_, schema_;
  Manifest manifest_;
  Origin origin_ = Origin::LegacyPush;
  char token_[33] = {};
  Phase phase_ = Phase::Idle;
  const char* reason_ = "NONE";
  uint32_t started_ = 0, progress_ = 0, received_ = 0;
};
const char* phaseName(Phase phase);
}}
