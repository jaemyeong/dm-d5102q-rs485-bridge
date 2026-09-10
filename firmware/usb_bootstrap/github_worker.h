#pragma once
#include "github_pull.h"
#include <Arduino.h>
#include <esp_tls.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <inttypes.h>
#include <time.h>

// Pinned IDF 4.4.7 ABI (libmbedtls.a). Arduino 2.0.17 shadows the same header
// name with arduino_esp_crt_bundle_attach, which needs a separately supplied
// bundle. Use the SDK's embedded, CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL
// bundle instead; keep this declaration in sync with the pinned SDK contract.
extern "C" esp_err_t esp_crt_bundle_attach(void* config);

namespace bootstrap { namespace github {
class Worker final : public Port {
#if defined(DM_HOST_TEST)
  friend struct WorkerHarness;
#endif
 public:
  bool start(const CheckRequest& request) override {
    if (!task_ && !initialize()) return false;
    cancelled_.store(false);
    return xQueueSend(requests_, &request, 0) == pdTRUE;
  }
  bool take(Message& message, uint32_t& receivedAt) override {
    if (!messages_ || xQueueReceive(messages_, &message, 0) != pdTRUE) return false;
    receivedAt = millis();
    return true;
  }
  void reply(uint32_t sequence, bool accepted) override {
    const Ack ack{sequence, accepted};
    // A single worker waits for exactly this response. Never block the loop.
    if (xQueueSend(acks_, &ack, 0) != pdTRUE) cancelled_.store(true);
  }
  void cancel() override { cancelled_.store(true); }
 private:
  struct Ack { uint32_t sequence; bool accepted; };
  static constexpr size_t kStackBytes = 16384;
  static constexpr uint32_t kConnectMs = 15000;
  bool initialize() {
    requests_ = xQueueCreateStatic(1, sizeof(CheckRequest), requestStorage_, &requestQueue_);
    messages_ = xQueueCreateStatic(2, sizeof(Message), messageStorage_, &messageQueue_);
    acks_ = xQueueCreateStatic(1, sizeof(Ack), ackStorage_, &ackQueue_);
    if (!requests_ || !messages_ || !acks_) return false;
    task_ = xTaskCreateStaticPinnedToCore(entry, "dm-github", kStackBytes, this, 1, stack_, &taskStorage_, 0);
    return task_ != nullptr;
  }
  static void entry(void* parameter) { static_cast<Worker*>(parameter)->run(); }
  void sampleResources() {
    const uint32_t heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const uint32_t block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (heap < result_.sampledMinHeap) result_.sampledMinHeap = heap;
    if (block < result_.sampledMinBlock) result_.sampledMinBlock = block;
  }
  bool alive() {
    sampleResources();
    return !cancelled_.load() && !elapsed(millis(), jobAt_, kJobMs) &&
      heap_caps_get_free_size(MALLOC_CAP_8BIT) >= 65536;
  }
  void close() {
    if (tls_) {
      // Read before destroy: the IDF error handle belongs to this connection.
      result_.tls.espError = esp_tls_get_and_clear_last_error(
        tls_->error_handle, &result_.tls.tlsError, &result_.tls.verifyFlags);
      esp_tls_conn_destroy(tls_);
    }
    tls_ = nullptr; rawUsed_ = rawSize_ = 0;
  }
  bool exchange(MessageKind kind, size_t size) {
    outgoing_.kind = kind; outgoing_.size = size; outgoing_.sequence = ++sequence_;
    outgoing_.created = millis(); outgoing_.result = result_;
    const uint32_t sentAt = millis();
    if (xQueueSend(messages_, &outgoing_, pdMS_TO_TICKS(100)) != pdTRUE)
      return exchangeFailed(ExchangeFailure::QueueSend, sentAt);
    const uint32_t at = millis();
    while (alive() && !elapsed(millis(), at, ota::kIdleMs)) {
      Ack ack;
      if (xQueueReceive(acks_, &ack, pdMS_TO_TICKS(20)) == pdTRUE && ack.sequence == sequence_)
        return ack.accepted ? true : exchangeFailed(ExchangeFailure::NegativeAck, at);
    }
    return exchangeFailed(cancelled_.load() ? ExchangeFailure::Cancelled :
      elapsed(millis(), jobAt_, kJobMs) ? ExchangeFailure::JobTimeout :
      heap_caps_get_free_size(MALLOC_CAP_8BIT) < 65536 ? ExchangeFailure::Resources : ExchangeFailure::AckTimeout, at);
  }
  bool exchangeFailed(ExchangeFailure failure, uint32_t at) {
    if (result_.exchangeFailure == ExchangeFailure::None) {
      result_.exchangeFailure = failure; result_.exchangeSequence = sequence_;
      result_.exchangeKind = static_cast<uint32_t>(outgoing_.kind) + 1;
      result_.exchangeWaitMs = millis() - at;
    }
    return false;
  }
  int rawByte() {
    while (alive()) {
      if (rawUsed_ < rawSize_) return raw_[rawUsed_++];
      if (elapsed(millis(), progressAt_, ota::kIdleMs)) return -1;
      const int count = esp_tls_conn_read(tls_, raw_, sizeof(raw_));
      if (count > 0) { rawSize_ = size_t(count); rawUsed_ = 0; progressAt_ = millis(); continue; }
      if (count != ESP_TLS_ERR_SSL_WANT_READ && count != ESP_TLS_ERR_SSL_WANT_WRITE) return -1;
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    return -1;
  }
  bool open(const Url& url, bool metadata, const char* etag, HttpHead& head) {
    close();
    if (!alive() || time(nullptr) < 1767225600) return false;
    const uint32_t jobAge = uint32_t(millis()) - jobAt_;
    if (jobAge >= kJobMs) return false;
    const uint32_t remaining = kJobMs - jobAge;
    esp_tls_cfg_t config{};
    // 302 hit the 3 s SDK budget before HTTP. Allow a bounded 15 s candidate,
    // capped by this job's remaining time (including redirects). The SDK may
    // overrun during a low-level step; alive() below still rejects late success.
    config.timeout_ms = int(remaining < kConnectMs ? remaining : kConnectMs);
    config.non_block = true;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    // common_name=null means exact hostname/SNI validation. Never disable it,
    // install a caller-provided CA, or permit plaintext when TLS fails.
    result_.tls = TlsDiagnostics{};
    result_.tls.attempted = true;
    const uint32_t connectAt = millis();
    tls_ = esp_tls_init();
    if (!tls_) {
      result_.tls.connectResult = -1; result_.tls.espError = ESP_ERR_NO_MEM;
      result_.tls.connectMs = uint32_t(millis()) - connectAt;
      return false;
    }
    result_.tls.connectResult = esp_tls_conn_new_sync(url.host, strlen(url.host), 443, &config, tls_);
    result_.tls.connectMs = uint32_t(millis()) - connectAt;
    if (result_.tls.connectResult != 1 || !alive()) return false;
    const int size = snprintf(buffer_, sizeof(buffer_),
      "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: dmbridge-ota/2\r\nAccept: %s\r\n"
      "X-GitHub-Api-Version: %s\r\nAccept-Encoding: identity\r\nConnection: close\r\n%s%s%s\r\n",
      url.path, url.host, metadata ? "application/vnd.github+json" : "application/octet-stream", kApiVersion,
      metadata && *etag ? "If-None-Match: " : "", metadata ? etag : "", metadata && *etag ? "\r\n" : "");
    if (size < 0 || size_t(size) >= sizeof(buffer_)) return false;
    progressAt_ = millis();
    for (size_t sent = 0; sent < size_t(size) && alive();) {
      if (elapsed(millis(), progressAt_, ota::kIdleMs)) return false;
      const int count = esp_tls_conn_write(tls_, buffer_ + sent, size_t(size) - sent);
      if (count > 0) { sent += size_t(count); progressAt_ = millis(); }
      else if (count == ESP_TLS_ERR_SSL_WANT_WRITE || count == ESP_TLS_ERR_SSL_WANT_READ) vTaskDelay(pdMS_TO_TICKS(1));
      else return false;
    }
    size_t used = 0;
    while (used < kHttpHeadMax) {
      const int value = rawByte(); if (value < 0) return false;
      buffer_[used++] = char(value);
      if (used >= 4 && !memcmp(buffer_ + used - 4, "\r\n\r\n", 4)) return parseHead(buffer_, used, head);
    }
    return false;
  }
  int bodyBytes(Body& body, uint8_t* bytes, size_t capacity) {
    if (!body.valid()) return -1;
    size_t count = 0;
    while (count < capacity && !body.complete()) {
      const int value = rawByte(); if (value < 0) return -1;
      bool payload;
      if (!body.feed(uint8_t(value), payload)) return -1;
      if (payload) bytes[count++] = uint8_t(value);
    }
    return int(count);
  }
  bool responsePolicy(const HttpHead& head, const CheckRequest& request) {
    result_.httpStatus = head.status;
    result_.waitMs = retryDelay(head, uint64_t(time(nullptr)), request.failures, esp_random());
    if (head.status == 403 || head.status == 429) { result_.code = ResultCode::RateLimit; return false; }
    return true;
  }
  void download(const CheckRequest& request) {
    Url url; strcpy(url.host, "api.github.com");
    snprintf(url.path, sizeof(url.path), "%s/releases/assets/%" PRIu64, kRepositoryPath, result_.release.asset);
    HttpHead head;
    for (unsigned hop = 0;; ++hop) {
      if (!open(url, false, "", head)) { result_.code = ResultCode::Network; return; }
      if (!responsePolicy(head, request)) return;
      if (head.status == 200) break;
      if ((head.status != 302 && head.status != 301 && head.status != 307 && head.status != 308) || hop >= 3 ||
          !parseUrl(head.location, url)) { result_.code = ResultCode::Http; return; }
    }
    if ((head.hasLength && head.length != result_.release.size) || (!head.hasLength && !head.chunked)) {
      result_.code = ResultCode::Metadata; return;
    }
    Body body(head, result_.release.size);
    if (bodyBytes(body, outgoing_.bytes, ota::kPackageHeaderBytes) != int(ota::kPackageHeaderBytes)) {
      result_.code = ResultCode::Network; return;
    }
    if (!exchange(MessageKind::Header, ota::kPackageHeaderBytes)) { result_.code = ResultCode::Rejected; return; }
    uint32_t left = result_.release.size - ota::kPackageHeaderBytes;
    while (left && alive()) {
      const size_t wanted = left < sizeof(outgoing_.bytes) ? left : sizeof(outgoing_.bytes);
      const int count = bodyBytes(body, outgoing_.bytes, wanted);
      if (count != int(wanted)) { result_.code = ResultCode::Network; return; }
      left -= uint32_t(count);
      if (!left) {
        uint8_t extra;
        if (bodyBytes(body, &extra, 1) != 0 || !body.complete()) { result_.code = ResultCode::Metadata; return; }
      }
      // Hold the last bytes until HTTP framing and total size also pass. The
      // main-loop writer selects boot when its last image chunk is accepted.
      if (!exchange(MessageKind::Chunk, size_t(count))) { result_.code = ResultCode::Rejected; return; }
    }
    result_.code = !left ? ResultCode::Updated : ResultCode::Cancelled;
  }
  void check(const CheckRequest& request) {
    if (!timeStarted_) { configTime(0, 0, "time.cloudflare.com", "pool.ntp.org"); timeStarted_ = true; }
    if (time(nullptr) < 1767225600) { result_.code = ResultCode::Time; return; }
    Url url; strcpy(url.host, "api.github.com"); strcpy(url.path, kLatestPath);
    HttpHead head;
    if (!open(url, true, request.etag, head)) { result_.code = ResultCode::Network; return; }
    if (!responsePolicy(head, request)) return;
    if (head.status == 404) { result_.code = ResultCode::NoRelease; return; }
    if (head.status == 304) { result_.code = request.etag[0] ? ResultCode::Unchanged : ResultCode::Metadata; return; }
    if (head.status != 200) { result_.code = ResultCode::Http; return; }
    if (!head.json) { result_.code = ResultCode::Metadata; return; }
    Body body(head, kJsonMax);
    const int size = bodyBytes(body, reinterpret_cast<uint8_t*>(buffer_), kJsonMax);
    uint8_t extra;
    if (size < 0 || bodyBytes(body, &extra, 1) != 0 || !body.complete() ||
        !parseRelease(buffer_, size_t(size), result_.release)) { result_.code = ResultCode::Metadata; return; }
    memcpy(result_.etag, head.etag, sizeof(result_.etag));
    close();
    if (result_.release.version <= request.floor) { result_.code = ResultCode::NoUpdate; return; }
    download(request);
  }
  void run() {
    for (;;) {
      CheckRequest request;
      if (xQueueReceive(requests_, &request, portMAX_DELAY) != pdTRUE) continue;
      execute(request);
      // Main cannot start another job until it receives Done. Retain this result
      // without polling the network again even when the main loop is stalled.
      while (xQueueSend(messages_, &outgoing_, pdMS_TO_TICKS(100)) != pdTRUE) vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  void execute(const CheckRequest& request) {
    result_ = Result{}; result_.sampledMinHeap = result_.sampledMinBlock = UINT32_MAX;
    jobAt_ = progressAt_ = millis();
    check(request); close(); sampleResources();
    if (cancelled_.load()) result_.code = ResultCode::Cancelled;
    if (result_.code == ResultCode::Network || result_.code == ResultCode::Metadata || result_.code == ResultCode::Time ||
        result_.code == ResultCode::Http || result_.code == ResultCode::Cancelled) {
      HttpHead failure; result_.waitMs = retryDelay(failure, 0, request.failures, esp_random());
    }
    // ESP-IDF 4.4 returns BYTES here, unlike upstream FreeRTOS. Do not multiply.
    result_.stackFreeBytes = uxTaskGetStackHighWaterMark(nullptr);
    outgoing_.kind = MessageKind::Done; outgoing_.size = 0; outgoing_.result = result_;
  }
  TaskHandle_t task_ = nullptr;
  QueueHandle_t requests_ = nullptr, messages_ = nullptr, acks_ = nullptr;
  StaticTask_t taskStorage_;
  StaticQueue_t requestQueue_, messageQueue_, ackQueue_;
  StackType_t stack_[kStackBytes / sizeof(StackType_t)];
  uint8_t requestStorage_[sizeof(CheckRequest)], messageStorage_[2 * sizeof(Message)], ackStorage_[sizeof(Ack)];
  std::atomic<bool> cancelled_{false};
  bool timeStarted_ = false;
  esp_tls_t* tls_ = nullptr;
  char buffer_[kJsonMax + 1];
  uint8_t raw_[1024];
  size_t rawUsed_ = 0, rawSize_ = 0;
  uint32_t jobAt_ = 0, progressAt_ = 0, sequence_ = 0;
  Result result_;
  Message outgoing_;
};
}}
