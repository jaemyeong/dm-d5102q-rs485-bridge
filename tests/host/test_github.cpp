#include "github_core.h"
#include "github_pull.h"
#include "vendor/monocypher/monocypher-ed25519.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <map>
#include <vector>
using namespace bootstrap::github;
static unsigned checks = 0;
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while (false)
const std::string release = R"({"id":123,"tag_name":"v0.3.1","draft":false,"prerelease":false,"body":"release notes \uD83D\uDE00","assets":[{"id":456,"name":"dmbridge-atom-lite-301.dmota","size":256,"state":"uploaded","ignored":{"list":[true,null,1.5,-2]}}]})";
void metadata() {
  Release result;
  CHECK(parseRelease(release.data(), release.size(), result));
  CHECK(result.id == 123 && result.asset == 456 && result.version == 301 && result.size == 256);
  for (size_t i = 0; i < release.size(); ++i) CHECK(!parseRelease(release.data(), i, result));
  for (const auto& substitution : {std::pair<std::string, std::string>{"\"draft\":false", "\"draft\":true"},
       {"\"prerelease\":false", "\"prerelease\":true"}, {"\"id\":123", "\"id\":123,\"id\":456"},
       {"301.dmota", "4294967296.dmota"}, {"\"size\":256", "\"size\":1310913"},
       {"\"uploaded\"", "\"new\""}, {"\"id\":456", "\"id\":0"}, {"v0.3.1", "bad/tag"}}) {
    auto changed = release; changed.replace(changed.find(substitution.first), substitution.first.size(), substitution.second);
    CHECK(!parseRelease(changed.data(), changed.size(), result));
  }
  const std::string extra = release + "garbage";
  CHECK(!parseRelease(extra.data(), extra.size(), result));
  const std::string huge(kJsonMax + 1, ' ');
  CHECK(!parseRelease(huge.data(), huge.size(), result));
}
void urlsAndHeaders() {
  Url url;
  CHECK(parseUrl("https://api.github.com/repos/jaemyeong/dm-d5102q-rs485-bridge/releases/latest", url));
  CHECK(parseUrl("https://release-assets.githubusercontent.com/asset?sig=fixture", url));
  for (const char* bad : {"http://api.github.com/x", "https://api.github.com.evil/x", "https://user@api.github.com/x",
       "https://api.github.com:443/x", "https://127.0.0.1/x", "https://github.com/x", "https://api.github.com/x\r\nHost:evil",
       "https://api.github.com/x#part", "https://api.github.com/\\evil"}) CHECK(!parseUrl(bad, url));
  HttpHead head;
  const auto parse = [&head](const std::string& value) { return parseHead(value.data(), value.size(), head); };
  CHECK(parse("HTTP/1.1 200 OK\r\nContent-Length: 256\r\nContent-Type: application/json; charset=utf-8\r\nETag: W/\"abc\"\r\n\r\n"));
  CHECK(head.status == 200 && head.length == 256 && head.json && !strcmp(head.etag, "W/\"abc\""));
  CHECK(!parse("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n"));
  CHECK(!parse("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n"));
  CHECK(!parse("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n"));
  CHECK(!parse("HTTP/1.1 200 OK\r\nContent-Length: 4294967296\r\n\r\n"));
  CHECK(!parse("HTTP/1.1 302 Found\r\nLocation: https://api.github.com/" + std::string(kUrlMax, 'a') + "\r\n\r\n"));
  CHECK(parse("HTTP/1.1 429 Too Many Requests\r\nRetry-After: 120\r\nX-RateLimit-Remaining: 0\r\nX-RateLimit-Reset: 1800\r\n\r\n"));
  CHECK(retryDelay(head, 1000, 0, 0) == 800000);
  head.retrySeconds = UINT32_MAX; CHECK(retryDelay(head, 1000, 0, 0) == UINT32_MAX);
  for (const unsigned status : {200, 304, 404}) {
    head = HttpHead{}; head.status = status;
    CHECK(retryDelay(head, 0, 0, 0) == 270000 && retryDelay(head, 0, 0, 60000) == 330000);
  }
  head.status = 503; CHECK(retryDelay(head, 0, 5, 0) > retryDelay(head, 0, 1, 0));
  Schedule schedule; schedule.begin(UINT32_MAX - 100, 0);
  CHECK(!schedule.due(29898) && schedule.due(29899));
  schedule.after(0, UINT32_MAX); CHECK(!schedule.due(UINT32_MAX));
}
void bodies() {
  HttpHead head; head.chunked = true;
  for (const std::string wire : {"3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n", "5\r\nabcde\r\n0\r\nX-Test: ignored\r\n\r\n"}) {
    Body body(head, 5); std::string data;
    for (size_t i = 0; i < wire.size(); ++i) {
      bool payload; CHECK(body.feed(uint8_t(wire[i]), payload));
      if (payload) data.push_back(wire[i]);
      CHECK(body.complete() == (i == wire.size() - 1));
    }
    CHECK(data == "abcde"); bool payload; CHECK(!body.feed('x', payload));
  }
  for (const std::string wire : {"6\r\nabcdef\r\n0\r\n\r\n", "ffffffffffffffff\r\n", "z\r\n", "1\na", "1\r\naXX"}) {
    Body body(head, 5); bool failed = false;
    for (const char byte : wire) { bool payload; if (!body.feed(uint8_t(byte), payload)) { failed = true; break; } }
    CHECK(failed && !body.complete());
  }
  head.chunked = false; head.hasLength = true; head.length = 5;
  Body body(head, 5); bool payload;
  for (unsigned i = 0; i < 5; ++i) CHECK(body.feed('a', payload) && payload);
  CHECK(body.complete() && !body.feed('x', payload));
  head.length = 6; CHECK(!Body(head, 5).valid());
}
struct Store : bootstrap::Storage {
  std::map<std::string, std::vector<uint8_t>> values;
  size_t read(const char* key, uint8_t* bytes, size_t capacity) override {
    if (!values.count(key)) return 0;
    const auto& value = values[key];
    if (value.size() > capacity) return SIZE_MAX;
    memcpy(bytes, value.data(), value.size()); return value.size();
  }
  bool write(const char* key, const uint8_t* bytes, size_t size) override {
    values[key] = std::vector<uint8_t>(bytes, bytes + size); return true;
  }
  bool erase(const char* key) override { values.erase(key); return true; }
};
struct Pipe : Port {
  unsigned starts = 0, cancels = 0;
  uint32_t dequeueAdvance = 0;
  CheckRequest request;
  std::vector<Message> messages;
  std::vector<bool> acknowledgements;
  bool start(const CheckRequest& value) override { ++starts; request = value; return true; }
  bool take(Message& value, uint32_t& receivedAt) override {
    if (messages.empty()) return false;
    receivedAt += dequeueAdvance; // Model time passing after caller sampled now.
    value = messages.front(); messages.erase(messages.begin()); return true;
  }
  void reply(uint32_t, bool value) override { acknowledgements.push_back(value); }
  void cancel() override { ++cancels; }
};
void token(char output[33]) { strcpy(output, "0123456789abcdef0123456789abcdef"); }
void coordinator() {
  using namespace bootstrap;
  for (unsigned kind = 0; kind < 5; ++kind) {
    Store store; Pipe pipe;
    uint8_t seed[32] = {9}, secret[64], pub[32]; crypto_ed25519_key_pair(secret, pub, seed);
    fakeOta() = FakeOta{};
    ota::Runtime runtime(store, pub); runtime.arm(0); runtime.begin(0);
    Pull pull(pipe, runtime, false); pull.begin(0, 0);
    pull.poll(60000, true, 0, token); CHECK(!pipe.starts); // Automatic default stays off.
    CHECK(pull.request(60000)); pull.poll(60000, true, 0, token); CHECK(pipe.starts == 1 && pull.busy());
    CHECK(!pull.request(60001));
    const std::string image(2300, 'a');
    ota::Manifest manifest; manifest.version = kOtaVersion + 1; manifest.imageSize = image.size(); manifest.minSchema = 1;
    manifest.minUpdater = 2; strcpy(manifest.boardId, ota::kBoardId); strcpy(manifest.buildId, "host-next"); strcpy(manifest.channel, "stable");
    mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(image.data()), image.size(), manifest.sha256, 0);
    Message header; header.kind = MessageKind::Header; header.created = 60000; header.size = ota::kPackageHeaderBytes;
    header.result.release.version = manifest.version; header.result.release.size = image.size() + ota::kPackageHeaderBytes;
    ota::encodePackageManifest(manifest, header.bytes);
    crypto_ed25519_sign(header.bytes + ota::kPackageManifestBytes, secret, header.bytes, ota::kPackageManifestBytes);
    if (kind == 1) header.bytes[191] ^= 1;
    if (kind == 2) ++header.result.release.version;
    if (kind == 3) ++header.result.release.size;
    pipe.messages.push_back(header); pull.poll(kind == 4 ? 70000 : 60001, true, 0, token);
    CHECK(pipe.acknowledgements.back() == (kind == 0));
    if (kind) { CHECK(!fakeOta().begins && !fakeOta().selects); continue; }
    CHECK(fakeOta().begins == 1 && store.values.count("otactx2"));
    for (size_t offset = 0; offset < image.size();) {
      Message chunk; chunk.kind = MessageKind::Chunk; chunk.created = 60002;
      chunk.size = image.size() - offset < 700 ? image.size() - offset : 700;
      memcpy(chunk.bytes, image.data() + offset, chunk.size); offset += chunk.size;
      pipe.messages.push_back(chunk); pull.poll(60003, true, 0, token);
      CHECK(pipe.acknowledgements.back());
    }
    CHECK(fakeOta().selects == 1 && !pull.rebootReady()); // TLS cleanup/Done first.
    Message done; done.result.code = ResultCode::Updated;
    pipe.messages.push_back(done); pull.poll(60004, true, 0, token);
    CHECK(!pull.busy() && pull.rebootReady());
    CHECK(store.values["ota"].size() == ota::kJournalBytes);
  }
  Store store; Pipe pipe; ota::Runtime runtime(store); runtime.arm(0); runtime.begin(0);
  Pull pull(pipe, runtime, true); pull.begin(UINT32_MAX - 100, 0);
  pull.poll(29898, true, 0, token); CHECK(!pipe.starts);
  pull.poll(29899, false, 0, token); CHECK(!pipe.starts);
  pull.poll(29899, true, 0, token); CHECK(pipe.starts == 1);
  Message done; done.result.code = ResultCode::RateLimit; done.result.waitMs = 800000;
  pipe.messages.push_back(done); pull.poll(30000, true, 0, token);
  CHECK(!pull.request(90000)); pull.poll(829999, true, 0, token); CHECK(pipe.starts == 1);
  pull.poll(830000, true, 0, token); CHECK(pipe.starts == 2);
  pull.poll(830000 + kJobMs, true, 0, token); CHECK(pipe.cancels == 1 && pull.busy());
  CHECK(!pull.request(830001 + kJobMs)); // Timed-out worker must finish before another owns the writer.
}
void automaticPolicy() {
  using namespace bootstrap;
  fakeOta() = FakeOta{};
  Store store; Pipe pipe; ota::Runtime runtime(store); runtime.arm(0); runtime.begin(0);
  Pull pull(pipe, runtime, false); pull.begin(0, 0);
  const auto before = store.values;
  const uint32_t base = UINT32_MAX - 100;
  pull.setAutomatic(true, base, 0);
  CHECK(pull.automatic() && pull.nextMs(base) == 30000);
  pull.setAutomatic(true, base + 10, 30000); // Idempotent ON never postpones.
  CHECK(pull.nextMs(base + 10) == 29990);
  pull.poll(base + 29999, true, 0, token); CHECK(!pipe.starts);
  pull.poll(base + 30000, false, 0, token); CHECK(!pipe.starts);
  pull.poll(base + 30000, true, 0, token); CHECK(pipe.starts == 1);
  pull.setAutomatic(false, base + 30001, 0);
  CHECK(!pull.automatic() && pull.busy() && !pipe.cancels);
  Message done; done.result.code = ResultCode::RateLimit; done.result.waitMs = 800000;
  pipe.messages.push_back(done); pull.poll(base + 30002, true, 0, token);
  pull.setAutomatic(true, base + 90000, 0);
  CHECK(!pull.request(base + 90000)); // Toggle cannot bypass Retry-After.
  pull.poll(base + 830001, true, 0, token); CHECK(pipe.starts == 1);
  pull.setAutomatic(false, base + 830002, 0);
  pull.poll(base + 830002, true, 0, token); CHECK(pipe.starts == 1);
  pull.setAutomatic(true, base + 830002, 0);
  pull.poll(base + 830002, true, 0, token); CHECK(pipe.starts == 2);
  done.result.code = ResultCode::NoUpdate; done.result.waitMs = kPollMs;
  pipe.messages.push_back(done); pull.poll(base + 830003, true, 0, token);
  pull.poll(base + 1130002, true, 0, token); CHECK(pipe.starts == 2);
  pull.poll(base + 1130003, true, 0, token); CHECK(pipe.starts == 3);
  pipe.messages.push_back(done); pull.poll(base + 1130004, true, 0, token);
  pull.setAutomatic(false, base + 1130005, 0);
  CHECK(pull.request(base + 1200000)); // Explicit manual work is independent.
  pull.setAutomatic(false, base + 1200001, 0);
  pull.poll(base + 1200001, true, 0, token); CHECK(pipe.starts == 4);
  CHECK(store.values == before && !fakeOta().begins && !pipe.cancels);
  Pull rebooted(pipe, runtime, false); CHECK(!rebooted.automatic());
}
void crossCoreMessageTime() {
  using namespace bootstrap;
  // Fresh chunk/header, wrap, idle expiry, job expiry, health loss, writer failure.
  for (unsigned kind = 0; kind < 8; ++kind) {
    const uint32_t base = kind == 2 || kind == 7 ? UINT32_MAX - 2 : 60000;
    Store store; Pipe pipe;
    uint8_t seed[32] = {9}, secret[64], pub[32]; crypto_ed25519_key_pair(secret, pub, seed);
    fakeOta() = FakeOta{};
    ota::Runtime runtime(store, pub); runtime.arm(0); runtime.begin(0);
    Pull pull(pipe, runtime, false); pull.begin(0, 0);
    CHECK(pull.request(base)); pull.poll(base, true, 0, token);
    const std::string image(32, 'a');
    ota::Manifest manifest; manifest.version = kOtaVersion + 1; manifest.imageSize = image.size(); manifest.minSchema = 1;
    manifest.minUpdater = 2; strcpy(manifest.boardId, ota::kBoardId); strcpy(manifest.buildId, "host-next"); strcpy(manifest.channel, "stable");
    mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(image.data()), image.size(), manifest.sha256, 0);
    Message header; header.kind = MessageKind::Header; header.created = base; header.size = ota::kPackageHeaderBytes;
    header.result.release.version = manifest.version; header.result.release.size = image.size() + ota::kPackageHeaderBytes;
    ota::encodePackageManifest(manifest, header.bytes);
    crypto_ed25519_sign(header.bytes + ota::kPackageManifestBytes, secret, header.bytes, ota::kPackageManifestBytes);
    if (kind == 1 || kind == 7) { header.created = base + 2; pipe.dequeueAdvance = 2; }
    pipe.messages.push_back(header); pull.poll(base + 1, true, 0, token);
    CHECK(pipe.acknowledgements.back() && runtime.updater.phase() == ota::Phase::Receiving);
    // The loop cachedbase+4; the other core enqueued atbase+5 before take().
    Message chunk; chunk.kind = MessageKind::Chunk; chunk.created = base + 5; chunk.size = image.size();
    memcpy(chunk.bytes, image.data(), chunk.size);
    pipe.dequeueAdvance = kind == 3 ? ota::kIdleMs + 1 : kind == 4 ? kJobMs : 2;
    if (kind == 6) fakeOta().writeError = ESP_FAIL;
    pipe.messages.push_back(chunk); pull.poll(base + 4, kind != 5, 0, token);
    const bool accepted = kind < 3 || kind == 7;
    CHECK(pipe.acknowledgements.back() == accepted);
    CHECK(fakeOta().selects == (accepted ? 1U : 0U));
    Message done; done.result.code = accepted ? ResultCode::Updated : ResultCode::Rejected;
    pipe.messages.push_back(done); pull.poll(base + 7, true, 0, token);
    CHECK(!pull.busy() && pull.rebootReady() == accepted);
    if (!accepted) {
      CHECK(runtime.updater.phase() == ota::Phase::Failed);
      CHECK(!strcmp(runtime.updater.reason(), kind == 6 ? "OTA_WRITE_FAILED" : "UPLOAD_INTERRUPTED"));
      CHECK(runtime.attemptVersion() == manifest.version && store.values.count("otactx2"));
    }
  }
}
int main() {
  metadata(); urlsAndHeaders(); bodies(); coordinator(); automaticPolicy(); crossCoreMessageTime();
  printf("%u GitHub parser/policy assertions passed\n", checks);
}
