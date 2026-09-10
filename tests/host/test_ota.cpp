#include "ota_runtime.h"
#include "vendor/monocypher/monocypher-ed25519.h"
#include <assert.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
using namespace bootstrap;
using namespace bootstrap::ota;
static unsigned checks = 0;
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while (false)
constexpr char token[] = "0123456789abcdef0123456789abcdef";
struct Store : Storage {
  std::map<std::string, std::vector<uint8_t>> values;
  bool failWrite = false;
  size_t read(const char* key, uint8_t* out, size_t capacity) override {
    if (!values.count(key)) return 0;
    auto& bytes = values[key]; if (bytes.size() > capacity) return SIZE_MAX;
    memcpy(out, bytes.data(), bytes.size()); return bytes.size();
  }
  bool write(const char* key, const uint8_t* bytes, size_t size) override {
    if (failWrite) return false;
    values[key] = std::vector<uint8_t>(bytes, bytes + size); return true;
  }
  bool erase(const char* key) override { values.erase(key); return true; }
};
struct Fixture {
  uint8_t secret[64], pub[32], envelope[kEnvelopeBytes];
  std::vector<uint8_t> image = std::vector<uint8_t>(2300, 0xa5);
  Manifest m;
  Store store;
  EspWriter writer{store};
  Updater updater{writer, pub, kOtaVersion, kConfigSchema};
  Fixture() {
    fakeOta() = FakeOta{};
    uint8_t seed[32] = {}; seed[0] = 9; // Public host-only test key, never device identity.
    crypto_ed25519_key_pair(secret, pub, seed);
    m.version = kOtaVersion + 1; m.imageSize = image.size(); m.minSchema = 1;
    strcpy(m.boardId, kBoardId); strcpy(m.buildId, "host-test-next");
    mbedtls_sha256_ret(image.data(), image.size(), m.sha256, 0); sign();
    store.values["enroll"] = {1,2,3}; store.values["active"] = {4,5,6};
  }
  void sign() { encodeManifest(m, envelope); crypto_ed25519_sign(envelope + kManifestBytes, secret, envelope, kManifestBytes); }
  void prepared(uint32_t now = 0) { CHECK(updater.prepare(envelope, sizeof(envelope), token, now)); }
  void started(uint32_t now = 0) { prepared(now); CHECK(updater.start(token, image.size(), now)); }
  bool send(uint32_t now = 1) {
    for (size_t off = 0; off < image.size();) {
      const size_t count = image.size() - off < 700 ? image.size() - off : 700;
      if (!updater.chunk(image.data() + off, count, now++)) return false;
      off += count;
    }
    return true;
  }
};
void verifiedFlow() {
  Fixture f; f.prepared(); CHECK(fakeOta().begins == 0);
  CHECK(!f.updater.start("bad-token", f.image.size(), 1)); CHECK(fakeOta().begins == 0);
  CHECK(f.updater.start(token, f.image.size(), 2));
  CHECK(!f.updater.start(token, f.image.size(), 3)); CHECK(fakeOta().begins == 1);
  CHECK(f.send(4)); CHECK(fakeOta().selects == 1 && fakeOta().ends == 1);
  CHECK(f.updater.phase() == Phase::RebootPending && fakeOta().image == f.image);
  f.updater.interrupt(); CHECK(fakeOta().aborts == 0);
  CHECK(f.store.values["enroll"] == std::vector<uint8_t>({1,2,3}));
  CHECK(f.store.values["active"] == std::vector<uint8_t>({4,5,6}));
  CHECK(!f.updater.prepare(f.envelope, sizeof(f.envelope), token, 20));
}
void rejection() {
  for (size_t i = 0; i < kEnvelopeBytes; ++i) {
    Fixture f; f.envelope[i] ^= 1;
    CHECK(!f.updater.prepare(f.envelope, sizeof(f.envelope), token, 0));
    CHECK(!fakeOta().begins && !fakeOta().writes && !fakeOta().selects);
  }
  for (unsigned kind = 0; kind < 6; ++kind) {
    Fixture f;
    if (kind == 0) strcpy(f.m.boardId, "wrong-board");
    if (kind == 1) f.m.minSchema = 2;
    if (kind == 2) f.m.version = kOtaVersion;
    if (kind == 3) f.m.imageSize = kImageMax + 1;
    if (kind == 4) f.m.imageSize = 0;
    if (kind == 5) f.m.version = kOtaVersion - 1;
    f.sign(); CHECK(!f.updater.prepare(f.envelope, sizeof(f.envelope), token, 0));
    CHECK(!fakeOta().begins);
  }
  Fixture f;
  CHECK(!f.updater.prepare(f.envelope, sizeof(f.envelope) - 1, token, 0));
  uint8_t empty[32] = {}; Updater disabled(f.writer, empty, 1, 1);
  CHECK(!disabled.enabled() && !disabled.prepare(f.envelope, sizeof(f.envelope), token, 0));
}
void failures() {
  for (unsigned kind = 0; kind < 9; ++kind) {
    Fixture f; f.started();
    if (kind == 0) fakeOta().writeError = 1;
    if (kind == 1) fakeOta().endError = 1;
    if (kind == 2) f.image[0] ^= 1;
    if (kind == 3) f.store.failWrite = true;
    if (kind == 4) { CHECK(!f.updater.chunk(f.image.data(), kChunkMax + 1, 1)); }
    else if (kind == 5) { f.updater.interrupt(); }
    else if (kind == 6) { f.updater.tick(kIdleMs); }
    else if (kind == 7) { CHECK(!f.updater.chunk(nullptr, 1, 1)); }
    else if (kind == 8) { CHECK(!f.updater.chunk(f.image.data(), 0, 1)); }
    else CHECK(!f.send());
    CHECK(f.updater.phase() == Phase::Failed && !fakeOta().selects);
    CHECK(!f.updater.start(token, f.image.size(), 10));
  }
  { Fixture f; f.prepared(); fakeOta().beginError = 1; CHECK(!f.updater.start(token, f.image.size(), 1)); CHECK(!fakeOta().selects); }
  { Fixture f; f.prepared(); CHECK(!f.updater.start(token, f.image.size() + 1, 1)); CHECK(!fakeOta().begins); }
  { Fixture f; f.prepared(UINT32_MAX - 100); CHECK(!f.updater.start(token, f.image.size(), kPrepareMs - 101)); CHECK(!fakeOta().begins); }
  { Fixture f; f.started(UINT32_MAX - 100); f.updater.tick(kIdleMs - 101); CHECK(f.updater.phase() == Phase::Failed); }
}
void health() {
  for (unsigned kind = 0; kind < 7; ++kind) {
    Fixture f;
    f.m.version = kOtaVersion; strcpy(f.m.buildId, kBuildId);
    CHECK(f.writer.select(f.m, token)); fakeOta().state = ESP_OTA_IMG_PENDING_VERIFY;
    if (kind == 1) f.store.values["ota"][0] ^= 1;
    if (kind == 2) f.store.values.erase("ota");
    if (kind == 3) { f.m.version++; CHECK(f.writer.select(f.m, token)); }
    Runtime runtime(f.store, f.pub); runtime.arm(UINT32_MAX - 100); runtime.begin(0);
    CHECK(!fakeOta().confirms);
    if (kind >= 1 && kind <= 3) { CHECK(fakeOta().rollbacks == 1 && !runtime.canUpdate()); continue; }
    CHECK(runtime.pending());
    CHECK(!runtime.confirm("bad", true, 1)); CHECK(!runtime.confirm(token, false, 1));
    if (kind == 4) {
      runtime.poll(kHealthMs - 101); CHECK(fakeOta().rollbacks == 1 && !fakeOta().confirms);
    } else if (kind == 5) {
      fakeOta().confirmError = 1; CHECK(!runtime.confirm(token, true, 1)); CHECK(runtime.pending());
    } else {
      CHECK(runtime.confirm(token, true, 1)); CHECK(fakeOta().confirms == 1 && runtime.canUpdate());
      CHECK(runtime.confirm(token, true, 2)); CHECK(fakeOta().confirms == 1);
    }
  }
  Fixture f; fakeOta().state = ESP_OTA_IMG_PENDING_VERIFY;
  Runtime runtime(f.store, f.pub); runtime.arm(0); // NVS/init failed before begin().
  runtime.poll(kHealthMs); CHECK(fakeOta().rollbacks == 1);
}
void packageV2() {
  for (size_t offset = 0; offset < kPackageHeaderBytes; ++offset) {
    Fixture f; uint8_t header[kPackageHeaderBytes];
    f.m.minUpdater = 2; strcpy(f.m.channel, "stable");
    encodePackageManifest(f.m, header);
    crypto_ed25519_sign(header + kPackageManifestBytes, f.secret, header, kPackageManifestBytes);
    header[offset] ^= 1;
    CHECK(!f.updater.preparePackage(header, sizeof(header), token, Origin::WebFile, 0));
    CHECK(!fakeOta().begins && !fakeOta().selects && !f.store.values.count("otactx2"));
  }
  for (unsigned kind = 0; kind < 6; ++kind) {
    Fixture f; uint8_t header[kPackageHeaderBytes];
    f.m.minUpdater = kind == 1 ? 3 : 2;
    strcpy(f.m.channel, kind == 2 ? "dev" : "stable");
    if (kind == 3) f.m.version = kOtaVersion;
    if (kind == 4) f.m.minSchema = 2;
    if (kind == 5) strcpy(f.m.boardId, "wrong-board");
    encodePackageManifest(f.m, header);
    crypto_ed25519_sign(header + kPackageManifestBytes, f.secret, header, kPackageManifestBytes);
    CHECK(!f.updater.preparePackage(header, sizeof(header), token,
                                   kind == 0 ? Origin::LegacyPush : Origin::WebFile, 0));
    CHECK(!fakeOta().begins);
  }
  for (const auto origin : {Origin::WebFile, Origin::GithubPull}) {
    Fixture f; uint8_t header[kPackageHeaderBytes];
    f.m.minUpdater = 2; strcpy(f.m.channel, "stable");
    encodePackageManifest(f.m, header);
    crypto_ed25519_sign(header + kPackageManifestBytes, f.secret, header, kPackageManifestBytes);
    CHECK(f.updater.preparePackage(header, sizeof(header), token, origin, 0));
    CHECK(f.updater.origin() == origin && !fakeOta().begins);
    CHECK(!f.updater.prepare(f.envelope, sizeof(f.envelope), token, 1));
    CHECK(f.updater.start(token, f.image.size(), 2));
    CHECK(f.store.values.count("otactx2") && !f.store.values.count("ota"));
    CHECK(f.send(3));
    CHECK(f.store.values["ota"].size() == kJournalBytes); // Old image's reader contract.
    Runtime previous(f.store, f.pub); previous.arm(0); previous.begin(0);
    CHECK(!strcmp(previous.bootState(), "ROLLED_BACK_OR_INTERRUPTED"));
    CHECK(previous.canUpdate()); // Management/recovery survives a failed candidate.
    CHECK(!previous.updater.preparePackage(header, sizeof(header), token, origin, 1));
    CHECK(!strcmp(previous.updater.reason(), "VERSION_QUARANTINED"));
  }
}
void localConfirmation() {
  for (unsigned kind = 0; kind < 8; ++kind) {
    Fixture f; f.m.version = kOtaVersion; strcpy(f.m.buildId, kBuildId);
    f.m.minUpdater = 2; strcpy(f.m.channel, "stable");
    CHECK(f.writer.authorize(f.m, token, Origin::WebFile));
    CHECK(f.writer.select(f.m, token));
    if (kind == 1) f.store.values["otactx2"][10] ^= 1;
    if (kind == 2) f.store.values["otactx2"].push_back(0);
    if (kind == 3) f.store.values.erase("otactx2"); // Missing context never opts legacy into self-confirm.
    fakeOta().state = ESP_OTA_IMG_PENDING_VERIFY;
    Runtime runtime(f.store, f.pub); runtime.arm(UINT32_MAX - 1000); runtime.begin(0);
    if (kind == 1 || kind == 2) { CHECK(fakeOta().rollbacks == 1); continue; }
    if (kind != 3) CHECK(!runtime.confirm(token, true, 0));
    for (uint32_t i = 0; i <= 5000; i += 100) runtime.poll(i, kind != 4);
    if (kind == 3 || kind == 4) {
      CHECK(!fakeOta().confirms);
      runtime.poll(kHealthMs); CHECK(fakeOta().rollbacks == 1);
    } else {
      CHECK(fakeOta().confirms == 1 && !runtime.pending());
      CHECK(!strcmp(runtime.bootState(), "VALID"));
    }
  }
  Fixture f; f.m.version = kOtaVersion; strcpy(f.m.buildId, kBuildId);
  CHECK(f.writer.authorize(f.m, token, Origin::GithubPull)); CHECK(f.writer.select(f.m, token));
  fakeOta().state = ESP_OTA_IMG_PENDING_VERIFY;
  Runtime runtime(f.store, f.pub); runtime.arm(0); runtime.begin(0);
  runtime.poll(1, true); runtime.poll(6000, true); // A stalled loop is not five seconds of health.
  CHECK(!fakeOta().confirms);
  for (uint32_t i = 6100; i <= 11000; i += 100) runtime.poll(i, true);
  CHECK(fakeOta().confirms == 1);
}
void interruptedContext() {
  for (unsigned kind = 0; kind < 4; ++kind) {
    Fixture f;
    if (kind == 0) f.store.failWrite = true;
    const bool accepted = f.writer.authorize(f.m, token, Origin::WebFile);
    CHECK(accepted == (kind != 0));
    if (kind == 0) { CHECK(!fakeOta().begins); continue; }
    // Power cut before erase, during image receive, or before boot selection:
    // only the atomic sidecar survives; the old ota blob is unchanged/absent.
    if (kind >= 2) CHECK(f.writer.begin(f.image.size()));
    if (kind == 3) CHECK(f.writer.write(f.image.data(), 100));
    f.writer.abort();
    Runtime reboot(f.store, f.pub); reboot.arm(0); reboot.begin(0);
    CHECK(reboot.canUpdate() && !fakeOta().selects);
    CHECK(!reboot.updater.prepare(f.envelope, sizeof(f.envelope), token, 0));
    CHECK(!strcmp(reboot.updater.reason(), "VERSION_QUARANTINED"));
    CHECK(f.store.values["enroll"] == std::vector<uint8_t>({1,2,3}));
    CHECK(f.store.values["active"] == std::vector<uint8_t>({4,5,6}));
  }
}
// Characterize the existing policy, not a proposed timeout increase. Continuous
// progress resets idle time only; the transfer deadline is absolute from start.
void transferDeadline() {
  for (const auto origin : {Origin::LegacyPush, Origin::WebFile, Origin::GithubPull}) {
    for (const uint32_t base : {15000U, UINT32_MAX - 60000U}) {
      for (unsigned kind = 0; kind < 3; ++kind) {
        Fixture f;
        f.image.assign(1048000, 0xa5); f.m.imageSize = f.image.size();
        mbedtls_sha256_ret(f.image.data(), f.image.size(), f.m.sha256, 0);
        // Preparing 29s earlier must not consume the receiving deadline.
        if (origin == Origin::LegacyPush) { f.sign(); f.prepared(base - 29000U); }
        else {
          uint8_t header[kPackageHeaderBytes];
          f.m.minUpdater = 2; strcpy(f.m.channel, "stable");
          encodePackageManifest(f.m, header);
          crypto_ed25519_sign(header + kPackageManifestBytes, f.secret, header, kPackageManifestBytes);
          CHECK(f.updater.preparePackage(header, sizeof(header), token, origin, base - 29000U));
        }
        CHECK(f.updater.start(token, f.image.size(), base));
        if (kind == 2) {
          CHECK(f.updater.chunk(f.image.data(), 1024, base + 100));
          f.updater.tick(base + 100 + kIdleMs - 1);
          CHECK(f.updater.phase() == Phase::Receiving);
          f.updater.tick(base + 100 + kIdleMs);
        } else {
          const size_t target = kind == 0 ? 754688 : f.image.size();
          const size_t chunks = (target + 1023) / 1024;
          for (size_t i = 0, off = 0; off < target; ++i) {
            const size_t count = target - off < 1024 ? target - off : 1024;
            const uint32_t age = uint64_t(i + 1) * (kTransferMs - 1) / chunks;
            CHECK(f.updater.chunk(f.image.data() + off, count, base + age));
            off += count;
          }
          CHECK(f.updater.received() == target);
          if (kind == 1) {
            CHECK(f.updater.phase() == Phase::RebootPending && fakeOta().selects == 1);
            continue;
          }
          CHECK(f.updater.phase() == Phase::Receiving);
          const unsigned writes = fakeOta().writes;
          CHECK(!f.updater.chunk(f.image.data() + target, 1024, base + kTransferMs));
          CHECK(fakeOta().writes == writes); // Even a fresh chunk is rejected.
        }
        CHECK(f.updater.phase() == Phase::Failed);
        CHECK(!strcmp(f.updater.reason(), "UPLOAD_TIMEOUT"));
        CHECK(fakeOta().aborts == 1 && !fakeOta().selects);
        f.updater.interrupt();
        CHECK(!strcmp(f.updater.reason(), "UPLOAD_TIMEOUT"));
      }
    }
  }
}
int main() {
  verifiedFlow(); rejection(); failures(); health();
  packageV2(); localConfirmation(); interruptedContext(); transferDeadline();
  printf("%u OTA assertions passed\n", checks);
}
