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
int main() {
  verifiedFlow(); rejection(); failures(); health();
  printf("%u OTA assertions passed\n", checks);
}
