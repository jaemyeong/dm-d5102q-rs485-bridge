#include "github_worker.h"
#include "vendor/monocypher/monocypher-ed25519.h"
#include <map>
#include <assert.h>
#include <stdio.h>
uint32_t fakeNow = 0;
uint32_t queueReceiveAdvance = 0;
FakeSerial Serial;
FakeEsp ESP;
TlsFake tlsFake;
std::function<void(QueueHandle_t)> queueSent;
static time_t testEpoch = 1780000000;
extern "C" time_t time(time_t* output) { if (output) *output = testEpoch; return testEpoch; }
extern "C" esp_err_t esp_crt_bundle_attach(void*) { return ESP_OK; }
void esp_fill_random(void* bytes, size_t size) { memset(bytes, 7, size); }
namespace bootstrap { namespace github {
struct WorkerHarness {
  static void rawSetup(Worker& worker, bool buffered) {
    worker.jobAt_ = worker.progressAt_ = fakeNow;
    worker.result_ = Result{};
    worker.result_.sampledMinHeap = worker.result_.sampledMinBlock = UINT32_MAX;
    worker.tls_ = new esp_tls_t;
    worker.tls_->response.assign(2048, 'x');
    memset(worker.raw_, 'x', sizeof(worker.raw_));
    worker.rawUsed_ = 0; worker.rawSize_ = buffered ? sizeof(worker.raw_) : 0;
  }
  static int raw(Worker& worker) { return worker.rawByte(); }
  static void closeRaw(Worker& worker) { worker.close(); }
  static Result guardedExchange(Worker& worker) {
    assert(worker.initialize());
    worker.jobAt_ = fakeNow; worker.result_ = Result{};
    assert(!worker.exchange(MessageKind::Chunk, 1024));
    assert(worker.messages_->data.empty()); // Reject BEFORE publishing to writer.
    return worker.result_;
  }
  static Result pendingExchange(Worker& worker, unsigned kind) {
    assert(worker.initialize()); worker.jobAt_ = fakeNow; worker.result_ = Result{};
    fakeQueueWait() = [&] {
      if (kind == 0) fakeFreeHeap() = 65535;
      else if (kind == 1) worker.cancel();
      else fakeNow = worker.jobAt_ + kJobMs;
    };
    assert(!worker.exchange(MessageKind::Chunk, 1024));
    fakeQueueWait() = {};
    return worker.result_;
  }
  static bool enqueue(Worker& worker, const Message& message) {
    return xQueueSend(worker.messages_, &message, 0) == pdTRUE;
  }
  static bool open(Worker& worker, uint32_t age, bool cancelled = false) {
    worker.jobAt_ = uint32_t(fakeNow) - age;
    worker.cancelled_.store(cancelled);
    Url url; strcpy(url.host, "api.github.com"); strcpy(url.path, kLatestPath);
    HttpHead head;
    const bool result = worker.open(url, true, "", head);
    worker.close();
    return result;
  }
  static bool messages(Worker& worker, QueueHandle_t queue) { return worker.messages_ == queue; }
  static bool chunk(Worker& worker) { return worker.outgoing_.kind == MessageKind::Chunk; }
  static Result exchangeFailure(Worker& worker, unsigned kind) {
    assert(worker.initialize());
    worker.jobAt_ = fakeNow; worker.result_ = Result{};
    if (kind == 0) {
      Message message;
      assert(enqueue(worker, message) && enqueue(worker, message));
    }
    if (kind == 2) worker.cancel();
    if (kind == 3) worker.jobAt_ = fakeNow - kJobMs;
    if (kind == 4) fakeFreeHeap() = 65535;
    assert(!worker.exchange(MessageKind::Header, 192));
    const Result first = worker.result_;
    worker.exchangeFailed(ExchangeFailure::NegativeAck, fakeNow);
    assert(worker.result_.exchangeFailure == first.exchangeFailure);
    fakeFreeHeap() = 160000;
    return first;
  }
  static Result execute(Worker& worker, bool cached = false, bool current = false) {
    CheckRequest request; assert(xQueueReceive(worker.requests_, &request, 0) == pdTRUE);
    if (cached) strcpy(request.etag, "\"fixture\"");
    if (current) ++request.floor;
    auto& previous = worker.result_.tls; // Must not leak from an earlier job.
    previous.attempted = true; previous.connectResult = -1; previous.connectMs = 99;
    previous.espError = 123; previous.tlsError = -456; previous.verifyFlags = 8;
    worker.result_.exchangeFailure = ExchangeFailure::QueueSend;
    worker.execute(request);
    assert(xQueueSend(worker.messages_, &worker.outgoing_, 0) == pdTRUE);
    return worker.result_;
  }
};
}}
using namespace bootstrap;
using namespace bootstrap::github;
static unsigned checks = 0;
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while (false)
struct Store : Storage {
  std::map<std::string, std::vector<uint8_t>> values;
  size_t read(const char* key, uint8_t* out, size_t size) override {
    if (!values.count(key)) return 0;
    const auto& bytes = values[key]; if (bytes.size() > size) return SIZE_MAX;
    memcpy(out, bytes.data(), bytes.size()); return bytes.size();
  }
  bool write(const char* key, const uint8_t* bytes, size_t size) override { values[key] = {bytes, bytes + size}; return true; }
  bool erase(const char* key) override { values.erase(key); return true; }
};
std::string response(const std::string& body, const char* type = "application/json") {
  return "HTTP/1.1 200 OK\r\nContent-Type: " + std::string(type) + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nETag: \"fixture\"\r\n\r\n" + body;
}
void token(char output[33]) { strcpy(output, "0123456789abcdef0123456789abcdef"); }
void samplingBoundaries() {
  // Buffered bytes require cheap cancellation/time checks, not heap queries.
  // A low heap during that bounded copy is rejected at refill or publication.
  for (unsigned kind = 0; kind < 10; ++kind) {
    fakeNow = UINT32_MAX - 100; tlsFake = TlsFake{};
    fakeFreeHeap() = 160000; fakeFreeHeapCalls() = fakeLargestBlockCalls() = 0;
    Worker worker; WorkerHarness::rawSetup(worker, kind < 3);
    if (kind == 0) {
      fakeFreeHeap() = 65535;
      for (unsigned i = 0; i < 1024; ++i) CHECK(WorkerHarness::raw(worker) == 'x');
      CHECK(fakeFreeHeapCalls() == 0 && fakeLargestBlockCalls() == 0);
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 0);
    } else if (kind == 1 || kind == 2) {
      if (kind == 1) worker.cancel(); else fakeNow += kJobMs;
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 0);
    } else if (kind == 3) {
      fakeFreeHeap() = 65535;
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 0);
    } else if (kind == 4 || kind == 5 || kind == 9) {
      tlsFake.wantReads = kind == 4 ? 0 : 1;
      if (kind == 9) tlsFake.wantCode = ESP_TLS_ERR_SSL_WANT_WRITE;
      tlsFake.afterRead = [] { fakeFreeHeap() = 65535; };
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 1);
    } else if (kind == 6 || kind == 7) {
      tlsFake.afterRead = [&] { if (kind == 6) worker.cancel(); else fakeNow += kJobMs; };
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 1);
    } else {
      fakeNow += ota::kIdleMs;
      CHECK(WorkerHarness::raw(worker) == -1 && tlsFake.readCalls == 0);
    }
    WorkerHarness::closeRaw(worker);
  }
  tlsFake = TlsFake{}; queueSent = {};
  { Worker worker; fakeFreeHeap() = 65535;
    CHECK(WorkerHarness::guardedExchange(worker).exchangeFailure == ExchangeFailure::Resources); }
  fakeFreeHeap() = 160000;
  for (unsigned kind = 0; kind < 3; ++kind) {
    Worker worker; fakeFreeHeap() = 160000; fakeNow = UINT32_MAX - 10;
    const Result result = WorkerHarness::pendingExchange(worker, kind);
    const ExchangeFailure expected[] = {ExchangeFailure::Resources, ExchangeFailure::Cancelled, ExchangeFailure::JobTimeout};
    CHECK(result.exchangeFailure == expected[kind]);
    CHECK(result.exchangeWaitMs == (kind == 2 ? kJobMs : 20));
  }
  fakeFreeHeap() = 160000;
}
int main() {
  samplingBoundaries();
  for (unsigned kind = 0; kind < 39; ++kind) {
    fakeNow = 60000; testEpoch = 1780000000; fakeOta() = FakeOta{}; tlsFake = TlsFake{};
    CHECK(fakeScratchLive() == 0);
    fakeScratchCalls() = 0; fakeScratchPeak() = 0;
    fakeFreeHeapCalls() = fakeLargestBlockCalls() = 0;
    if (kind == 38) tlsFake.readFragment = 1024;
    fakeScratchFailAt() = kind >= 32 && kind <= 34 ? kind - 31 : 0;
    queueReceiveAdvance = kind >= 24 ? 2 : 0;
    if (kind == 25) fakeNow = UINT32_MAX - 30;
    uint8_t seed[32] = {9}, secret[64], pub[32]; crypto_ed25519_key_pair(secret, pub, seed);
    std::string image(kind >= 37 ? 1048000 : 2300, 'x');
    ota::Manifest manifest; manifest.version = kOtaVersion + 1; manifest.imageSize = image.size(); manifest.minSchema = 1;
    manifest.minUpdater = 2; strcpy(manifest.boardId, ota::kBoardId); strcpy(manifest.buildId, "worker-test"); strcpy(manifest.channel, "stable");
    mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(image.data()), image.size(), manifest.sha256, 0);
    uint8_t header[ota::kPackageHeaderBytes]; ota::encodePackageManifest(manifest, header);
    crypto_ed25519_sign(header + ota::kPackageManifestBytes, secret, header, ota::kPackageManifestBytes);
    std::string package(reinterpret_cast<const char*>(header), sizeof(header)); package += image;
    const std::string json = "{\"id\":1,\"tag_name\":\"v0.3.1\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"id\":2,\"name\":\"dmbridge-atom-lite-" +
      std::to_string(kOtaVersion + 1) + ".dmota\",\"size\":" + std::to_string(package.size()) + ",\"state\":\"uploaded\"}]}";
    tlsFake.responses.push_back(response(json));
    if (kind == 1) tlsFake.responses.push_back("HTTP/1.1 302 Found\r\nLocation: https://release-assets.githubusercontent.com/test?sig=fixture\r\nContent-Length: 0\r\n\r\n");
    if (kind == 2) package[191] ^= 1;
    if (kind == 3) package.back() ^= 1;
    if (kind == 4) testEpoch = 0;
    if (kind == 5) tlsFake.failConnect = true;
    std::string download = response(package, "application/octet-stream");
    if (kind == 6) download.resize(download.size() - 1);
    if (kind == 7) download = "HTTP/1.1 302 Found\r\nLocation: http://release-assets.githubusercontent.com/x\r\n\r\n";
    if (kind == 8 || kind == 12) {
      char hex[32]; snprintf(hex, sizeof(hex), "%x", unsigned(package.size()));
      download = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + std::string(hex) + "\r\n" + package + "\r\n"; // Missing zero-chunk: never select.
      if (kind == 12) download += "0\r\n\r\n";
    }
    if (kind == 9) tlsFake.responses.front() = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    if (kind == 10) tlsFake.responses.front() = "HTTP/1.1 429 Too Many Requests\r\nRetry-After: 800\r\nContent-Length: 0\r\n\r\n";
    if (kind == 11) tlsFake.responses.front() = response(std::string(kJsonMax + 1, 'x'));
    if (kind == 13 || kind == 14) tlsFake.responses.front() = "HTTP/1.1 304 Not Modified\r\nContent-Length: 0\r\n\r\n";
    if (kind == 16) tlsFake.responses.front() = "HTTP/1.1 403 Forbidden\r\nX-RateLimit-Remaining: 0\r\nX-RateLimit-Reset: " + std::to_string(testEpoch + 900) + "\r\nContent-Length: 0\r\n\r\n";
    if (kind == 17) for (unsigned hop = 0; hop < 4; ++hop)
      tlsFake.responses.push_back("HTTP/1.1 302 Found\r\nLocation: https://release-assets.githubusercontent.com/loop\r\nContent-Length: 0\r\n\r\n");
    if (kind == 18) download = "HTTP/1.1 302 Found\r\nLocation: https://evil.example/x\r\n\r\n";
    if (kind == 19) { tlsFake.timeoutConnect = true; tlsFake.connectDelayMs = 15001; tlsFake.error = {0x8006, 0, 0}; }
    if (kind == 20) tlsFake.failInit = true;
    if (kind == 21) {
      fakeNow = UINT32_MAX - 6;
      tlsFake.responses.front() = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    }
    if (kind == 22) tlsFake.failWrite = true;
    if (kind == 23) tlsFake.failRead = true;
    if (kind == 35) tlsFake.responses.front() = response(json + std::string(kJsonMax - json.size(), ' '));
    if (kind == 36) {
      const size_t end = download.find("\r\n\r\n");
      size_t extra = kHttpHeadMax - (end + 4);
      std::string padding;
      while (extra > 1500) { padding += "X-Pad: " + std::string(991, 'x') + "\r\n"; extra -= 1000; }
      padding += "X-Pad: " + std::string(extra - 9, 'x') + "\r\n";
      download.insert(end + 2, padding);
      CHECK(download.find("\r\n\r\n") + 4 == kHttpHeadMax);
    }
    tlsFake.responses.push_back(download);
    Store store; Worker worker; ota::Runtime runtime(store, pub); runtime.arm(fakeNow); runtime.begin(fakeNow);
    Pull pull(worker, runtime, false); pull.begin(fakeNow, 0);
    unsigned imageMessages = 0;
    queueSent = [&](QueueHandle_t queue) {
      // Producer can enqueue after loop cached now; dequeue itself can advance
      // the clock. Exercise the real Worker's post-dequeue clock sampling.
      if (kind == 26 || !WorkerHarness::messages(worker, queue)) return;
      const bool chunk = WorkerHarness::chunk(worker);
      CHECK(fakeScratchLive() == 0); // Header, Chunk and Done must own no scratch.
      if (chunk) ++imageMessages;
      // Model the loop's health input dropping only after one accepted chunk,
      // or at the final chunk. This is not a measurement of target heap timing.
      const bool healthy = !((kind == 27 && imageMessages == 2) ||
                             (kind == 28 && imageMessages == 3));
      if (chunk && kind == 29 && imageMessages == 2) fakeOta().writeError = ESP_FAIL;
      if (chunk && kind == 30) fakeOta().endError = ESP_FAIL;
      if (chunk && kind == 31) fakeOta().selectError = ESP_FAIL;
      HealthSample sample; sample.heap = healthy ? 160000 : 80156;
      sample.block = 69620; sample.failed = healthy ? 0 : 64;
      pull.poll(kind >= 24 ? fakeNow - 1 : fakeNow, healthy, 0, token, sample);
    };
    CHECK(pull.request(fakeNow)); pull.poll(fakeNow, true, 0, token);
    const Result result = WorkerHarness::execute(worker, kind == 14, kind == 15);
    if (kind >= 37) {
      // Count real Worker operations; fake heap/TLS timings are NOT ESP32 timings.
      CHECK(imageMessages == (image.size() + 1023) / 1024);
      CHECK(tlsFake.readBytes > image.size());
      CHECK(fakeFreeHeapCalls() + fakeLargestBlockCalls() < image.size() / 2);
      CHECK(fakeLargestBlockCalls() < image.size() / 4);
      CHECK(tlsFake.readCalls < fakeLargestBlockCalls());
      printf("worker cost: fragment=%zu image=%zu heap=%zu block=%zu TLSreads=%zu TLSbytes=%zu imageACKs=%u\n",
        tlsFake.readFragment, image.size(), fakeFreeHeapCalls(), fakeLargestBlockCalls(),
        tlsFake.readCalls, tlsFake.readBytes, imageMessages);
    }
    CHECK(fakeScratchLive() == 0);
    CHECK(fakeScratchPeak() <= kJsonMax + 1);
    if (kind == 0) CHECK(fakeScratchCalls() == 3 && fakeScratchPeak() == kJsonMax + 1);
    if (kind >= 32 && kind <= 34) CHECK(result.code == ResultCode::Resources && !fakeOta().begins && !fakeOta().writes && !fakeOta().selects);
    CHECK(tlsFake.allocated == tlsFake.destroyed);
    CHECK(tlsFake.errorsRead == tlsFake.allocated);
    CHECK(result.tls.attempted == (kind != 4));
    if (kind == 4) {
      CHECK(result.tls.connectResult == 0 && result.tls.connectMs == 0);
      CHECK(result.tls.espError == 0 && result.tls.tlsError == 0 && result.tls.verifyFlags == 0);
    } else if (kind == 20) {
      CHECK(result.tls.connectResult == -1 && result.tls.espError == ESP_ERR_NO_MEM);
      CHECK(result.tls.connectMs == 0 && !tlsFake.allocated);
    } else {
      CHECK(result.tls.connectMs == tlsFake.connectDelayMs);
      CHECK(result.tls.connectResult == (kind == 5 ? -1 : kind == 19 ? 0 : 1));
      const bool error = kind == 5 || kind == 19 || kind == 22 || kind == 23;
      CHECK(result.tls.espError == (error ? tlsFake.error.error : 0));
      CHECK(result.tls.tlsError == (error ? tlsFake.error.code : 0));
      CHECK(result.tls.verifyFlags == (error ? tlsFake.error.flags : 0));
    }
    const bool updated = kind < 2 || kind == 12 || kind == 24 || kind == 25 || kind >= 35;
    if (kind >= 35) fprintf(stderr, "maximum-buffer case%u result%s reason%s\n", kind, resultName(result.code), runtime.updater.reason());
    CHECK(fakeOta().selects == (updated || kind == 31 ? 1U : 0U));
    CHECK(pull.rebootReady() == updated);
    if (updated) { CHECK(result.code == ResultCode::Updated); CHECK(fakeOta().image == std::vector<uint8_t>(image.begin(), image.end())); }
    if (updated || kind == 4) CHECK(result.exchangeFailure == ExchangeFailure::None && !result.exchangeSequence);
    if (kind == 4) CHECK(tlsFake.hosts.empty() && result.code == ResultCode::Time);
    if (kind == 9) CHECK(result.code == ResultCode::NoRelease && !fakeOta().begins);
    if (kind == 10) CHECK(result.code == ResultCode::RateLimit && result.waitMs >= 800000 && result.waitMs < 801000);
    if (kind == 11) CHECK(result.code == ResultCode::Metadata && !fakeOta().begins);
    if (kind == 13) CHECK(result.code == ResultCode::Metadata && !fakeOta().begins);
    if (kind == 14) {
      CHECK(result.code == ResultCode::Unchanged && !fakeOta().begins);
      CHECK(tlsFake.requests.front().find("If-None-Match: \"fixture\"") != std::string::npos);
    }
    if (kind == 15) CHECK(result.code == ResultCode::NoUpdate && tlsFake.hosts.size() == 1 && !fakeOta().begins);
    if (kind == 16) CHECK(result.code == ResultCode::RateLimit && result.waitMs >= 900000);
    if (kind == 17) CHECK(result.code == ResultCode::Http && tlsFake.hosts.size() == 5 && !fakeOta().begins);
    if (kind == 18) CHECK(result.code == ResultCode::Http && tlsFake.hosts.size() == 2 && !fakeOta().begins);
    if (kind == 5 || kind == 19 || kind == 20 || kind == 22 || kind == 23)
      CHECK(result.code == ResultCode::Network && !fakeOta().begins && !fakeOta().writes && !fakeOta().selects);
    if (kind == 21) CHECK(result.code == ResultCode::NoRelease && !fakeOta().begins);
    if (kind == 26) CHECK(result.code == ResultCode::Rejected && fakeNow >= 60000 + ota::kIdleMs && !fakeOta().begins);
    if (kind >= 27 && kind <= 31) {
      const char* reason = kind <= 28 ? "UPLOAD_INTERRUPTED" : kind == 29 ?
        "OTA_WRITE_FAILED" : kind == 30 ? "IMAGE_INVALID" : "BOOT_SELECT_FAILED";
      CHECK(result.code == ResultCode::Rejected && result.httpStatus == 200);
      CHECK(result.exchangeFailure == ExchangeFailure::NegativeAck);
      CHECK(result.exchangeKind == 2);
      CHECK(pull.rejection().present);
      CHECK(pull.rejection().kind == MessageKind::Chunk);
      CHECK(pull.rejection().received == runtime.updater.received());
      CHECK((pull.rejection().gates & 4U) == (kind <= 28 ? 4U : 0U));
      CHECK(pull.rejection().health.heap == (kind <= 28 ? 80156U : 160000U));
      CHECK(pull.rejection().health.failed == (kind <= 28 ? 64U : 0U));
      CHECK(runtime.updater.phase() == ota::Phase::Failed);
      CHECK(!strcmp(runtime.updater.reason(), reason));
      CHECK(fakeOta().begins == 1 && !pull.rebootReady());
      CHECK(runtime.attemptVersion() == manifest.version && store.values.count("otactx2"));
      CHECK(fakeOta().writes == (kind == 27 ? 1U : kind <= 29 ? 2U : 3U));
      CHECK(fakeOta().ends == (kind >= 30 ? 1U : 0U));
      CHECK(!fakeOta().confirms && !fakeOta().rollbacks);
      printf("negative-ACK case %u: %s, accepted bytes %u\n", kind, reason, runtime.updater.received());
      const auto first = pull.rejection();
      pull.poll(fakeNow + 1, true, 0, token);
      CHECK(pull.rejection().sequence == first.sequence && pull.rejection().gates == first.gates);
      CHECK(pull.request(fakeNow + 60000));
      pull.poll(fakeNow + 60000, true, 0, token);
      CHECK(!pull.rejection().present);
    }
    for (const auto& request : tlsFake.requests) { CHECK(request.find("Authorization:") == std::string::npos); CHECK(request.find("Accept-Encoding: identity") != std::string::npos || request.empty()); }
    queueSent = {};
  }
  queueReceiveAdvance = 0;
  fakeScratchFailAt() = 0;
  for (unsigned kind = 0; kind < 5; ++kind) {
    Worker worker; fakeNow = 60000;
    const Result result = WorkerHarness::exchangeFailure(worker, kind);
    const ExchangeFailure expected[] = {ExchangeFailure::QueueSend, ExchangeFailure::AckTimeout,
      ExchangeFailure::Cancelled, ExchangeFailure::JobTimeout, ExchangeFailure::Resources};
    CHECK(result.exchangeFailure == expected[kind] && result.exchangeSequence == 1);
    CHECK(result.exchangeKind == 1);
    if (kind == 1) CHECK(result.exchangeWaitMs == ota::kIdleMs);
  }
  {
    Worker worker; Message incoming, outgoing; uint32_t receivedAt = 123;
    CHECK(!worker.take(outgoing, receivedAt) && receivedAt == 123);
    CHECK(worker.start(CheckRequest{}));
    incoming.created = 100; incoming.sequence = 7;
    CHECK(WorkerHarness::enqueue(worker, incoming));
    fakeNow = 100; queueReceiveAdvance = 2;
    CHECK(worker.take(outgoing, receivedAt));
    CHECK(receivedAt == 102 && receivedAt == fakeNow && outgoing.sequence == 7);
    CHECK(!worker.take(outgoing, receivedAt) && receivedAt == 102);
    queueReceiveAdvance = 0;
  }
  // Exercise the real adapter's budget calculation, not SDK handshake timing.
  static_assert(kJobMs == 180000, "Keep the global job limit");
  static_assert(ota::kIdleMs == 8000, "Keep the HTTP/writer idle limit");
  for (unsigned kind = 0; kind < 8; ++kind) {
    fakeNow = kind == 7 ? UINT32_MAX - 100 : 60000;
    testEpoch = 1780000000; tlsFake = TlsFake{};
    tlsFake.responses.push_back("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
    tlsFake.connectDelayMs = kind == 0 || kind == 7 ? 5676 : kind == 5 ? 250 : 0;
    const uint32_t age = kind == 1 || kind == 5 ? kJobMs - 250 :
      kind == 2 ? kJobMs - 1 : kind == 3 ? kJobMs : kind == 4 ? kJobMs + 1 : 0;
    Worker worker;
    const bool opened = WorkerHarness::open(worker, age, kind == 6);
    const bool skipped = kind == 3 || kind == 4 || kind == 6;
    CHECK(opened == (!skipped && kind != 5));
    CHECK(tlsFake.timeouts.size() == (skipped ? 0U : 1U));
    if (!skipped) CHECK(tlsFake.timeouts.front() == (kind == 1 || kind == 5 ? 250 : kind == 2 ? 1 : 15000));
    if (kind == 5) CHECK(tlsFake.requests.front().empty()); // Late success must not send HTTP.
    CHECK(tlsFake.allocated == tlsFake.destroyed && tlsFake.errorsRead == tlsFake.allocated);
  }
  printf("%u GitHub worker/TLS adapter fake assertions passed (not real TLS or RTOS timing)\n", checks);
}
