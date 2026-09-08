#pragma once
#include "ota_core.h"
#include "ota_context.h"
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <string.h>

// Only a public verification key is injected. Missing identity disables OTA.
#if defined(DM_HOST_TEST)
// RFC 8032 public test vector, rejected by config.h in any device build.
static constexpr uint8_t kOtaPublicKey[32] = {
  0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
  0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
#elif __has_include("../../.arduino/tmp/ota_identity.h")
#include "../../.arduino/tmp/ota_identity.h"
#else
static constexpr uint8_t kOtaPublicKey[32] = {};
#endif

namespace bootstrap { namespace ota {
constexpr size_t kJournalBytes = kManifestBytes + 32 + 4;
class EspWriter final : public ImageWriter {
 public:
  explicit EspWriter(Storage& storage) : storage_(storage) {}
  bool available(const Manifest& manifest) override {
    Context prior;
    const auto result = readContext(storage_, prior);
    if (result == ContextRead::Valid) attemptVersion_ = prior.manifest.version;
    // An accepted attempt is a durable quarantine fence, not a pre-boot
    // high-water mark in efuse. The SDK may still roll back to the old slot.
    return result == ContextRead::Missing ||
      (result == ContextRead::Valid && manifest.version > prior.manifest.version);
  }
  bool authorize(const Manifest& manifest, const char* transaction, Origin origin) override {
    authorized_ = Context{};
    if (!validToken(transaction)) return false;
    if (origin == Origin::LegacyPush) return true;
    if ((origin != Origin::WebFile && origin != Origin::GithubPull) || !available(manifest)) return false;
    Context context; context.manifest = manifest; context.origin = origin;
    memcpy(context.transaction, transaction, sizeof(context.transaction));
    // Commit before the first erase/write. Even a failed begin or a power cut
    // cannot cause autonomous reinstallation of this same version forever.
    if (!writeContext(storage_, context)) return false;
    authorized_ = context; attemptVersion_ = manifest.version;
    return true;
  }
  uint32_t attemptVersion() const { return attemptVersion_; }
  void resetForBoot() { abort(); authorized_ = Context{}; attemptVersion_ = 0; }
  bool begin(uint32_t size) override {
    partition_ = esp_ota_get_next_update_partition(nullptr);
    const auto* running = esp_ota_get_running_partition();
    if (!partition_ || !running || partition_->address == running->address || size > partition_->size) return false;
    if (esp_ota_begin(partition_, size, &handle_) != ESP_OK) return false;
    active_ = true;
    mbedtls_sha256_init(&hash_); hashActive_ = true;
    return mbedtls_sha256_starts_ret(&hash_, 0) == 0;
  }
  bool write(const uint8_t* bytes, size_t size) override {
    return active_ && esp_ota_write(handle_, bytes, size) == ESP_OK &&
      mbedtls_sha256_update_ret(&hash_, bytes, size) == 0;
  }
  bool finish(uint8_t sha256[32]) override {
    if (!active_) return false;
    const bool hashed = mbedtls_sha256_finish_ret(&hash_, sha256) == 0;
    mbedtls_sha256_free(&hash_); hashActive_ = false;
    // esp_ota_end frees the handle even on failure.
    active_ = false;
    return esp_ota_end(handle_) == ESP_OK && hashed;
  }
  bool select(const Manifest& manifest, const char* transaction) override {
    if (authorized_.origin != Origin::LegacyPush) {
      Context saved;
      if (!contextMatches(authorized_, manifest, transaction) ||
          readContext(storage_, saved) != ContextRead::Valid ||
          !contextMatches(saved, manifest, transaction) || saved.origin != authorized_.origin) return false;
    }
    uint8_t bytes[kJournalBytes] = {}, check[kJournalBytes];
    encodeManifest(manifest, bytes); memcpy(bytes + kManifestBytes, transaction, 32);
    const uint32_t crc = crc32(bytes, kJournalBytes - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[kJournalBytes - 4 + i] = static_cast<uint8_t>(crc >> (8 * i));
    // A checked atomic NVS blob precedes boot selection; Wi-Fi/enroll are untouched.
    if (!storage_.write("ota", bytes, sizeof(bytes)) ||
        storage_.read("ota", check, sizeof(check)) != sizeof(check) || memcmp(bytes, check, sizeof(bytes))) return false;
    return esp_ota_set_boot_partition(partition_) == ESP_OK;
  }
  void abort() override {
    if (active_) esp_ota_abort(handle_);
    if (hashActive_) mbedtls_sha256_free(&hash_);
    active_ = hashActive_ = false;
  }
 private:
  Storage& storage_;
  Context authorized_;
  uint32_t attemptVersion_ = 0;
  esp_ota_handle_t handle_ = 0;
  const esp_partition_t* partition_ = nullptr;
  mbedtls_sha256_context hash_;
  bool active_ = false, hashActive_ = false;
};

class Runtime {
 public:
  explicit Runtime(Storage& storage, const uint8_t* key = kOtaPublicKey)
    : writer(storage), updater(writer, key, kOtaVersion, kConfigSchema), storage_(storage) {}
  void arm(uint32_t now) {
    updater.resetForBoot();
    writer.resetForBoot();
    pending_ = fatal_ = contextError_ = observing_ = false; started_ = now; state_ = "USB_BASELINE";
    origin_ = Origin::LegacyPush; healthyAt_ = lastPoll_ = now; attemptVersion_ = 0;
    transaction_[0] = 0; expected_ = Manifest{};
    const auto* running = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    const auto status = running ? esp_ota_get_state_partition(running, &imageState) : ESP_FAIL;
    if (!running || (status != ESP_OK && status != ESP_ERR_NOT_FOUND)) { fatal_ = true; state_ = "BOOT_STATE_ERROR"; return; }
    pending_ = status == ESP_OK && imageState == ESP_OTA_IMG_PENDING_VERIFY;
    const auto init = esp_task_wdt_init(10, true);
    if (init != ESP_OK ||
        (esp_task_wdt_status(nullptr) != ESP_OK && esp_task_wdt_add(nullptr) != ESP_OK)) {
      fatal_ = true; state_ = "WATCHDOG_ERROR";
    }
  }
  void begin(uint32_t) {
    if (fatal_) return;
    Context context;
    const auto contextRead = readContext(storage_, context);
    contextError_ = contextRead == ContextRead::Invalid;
    if (contextRead == ContextRead::Valid) attemptVersion_ = context.manifest.version;
    uint8_t bytes[kJournalBytes];
    const size_t size = storage_.read("ota", bytes, sizeof(bytes));
    if (size) {
      uint32_t crc = 0;
      if (size == sizeof(bytes))
        for (unsigned i = 0; i < 4; ++i) crc |= uint32_t(bytes[sizeof(bytes) - 4 + i]) << (8 * i);
      if (size != sizeof(bytes) || crc != crc32(bytes, sizeof(bytes) - 4) ||
          !decodeManifest(bytes, kManifestBytes, expected_)) { fatal_ = true; state_ = "OTA_JOURNAL_ERROR"; }
      else {
        memcpy(transaction_, bytes + kManifestBytes, 32); transaction_[32] = 0;
        if (!validToken(transaction_)) { fatal_ = true; state_ = "OTA_JOURNAL_ERROR"; }
        else state_ = matches() ? "VALID" : "ROLLED_BACK_OR_INTERRUPTED";
      }
    }
    if (contextRead == ContextRead::Valid && contextMatches(context, expected_, transaction_)) origin_ = context.origin;
    if (pending_) {
      if (fatal_ || contextError_ || !size || !matches()) { rollback(); return; }
      state_ = "PENDING_VERIFY";
    }
    else if (contextError_) state_ = "OTA_CONTEXT_ERROR";
  }
  void poll(uint32_t now, bool healthy = false) {
    updater.tick(now);
    if (pending_ && (fatal_ || elapsed(now, started_, kHealthMs))) { rollback(); return; }
    if (!pending_ || origin_ == Origin::LegacyPush) return;
    if (!healthy || elapsed(now, lastPoll_, kHealthPollMaxMs)) observing_ = false;
    lastPoll_ = now;
    if (!healthy) return;
    if (!observing_) { observing_ = true; healthyAt_ = now; }
    if (elapsed(now, healthyAt_, kLocalHealthMs)) confirm(transaction_, true, now);
  }
  bool confirm(const char* transaction, bool healthy, uint32_t now) {
    if (fatal_ || !validToken(transaction) || !equalSecret(transaction_, transaction) || !matches()) return false;
    if (!pending_) return !strcmp(state_, "VALID"); // Idempotent ACK after a lost response.
    if (!healthy || elapsed(now, started_, kHealthMs)) return false;
    if (origin_ != Origin::LegacyPush && (!observing_ || !elapsed(now, healthyAt_, kLocalHealthMs) ||
        elapsed(now, lastPoll_, kHealthPollMaxMs))) return false;
    if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) return false;
    pending_ = false; state_ = "VALID";
    return true;
  }
  void heartbeat() {
    // The watchdog measures loop liveness, not a recoverable business fault.
    // Keep a responsive faulted device available; do not create a reset loop.
    if (esp_task_wdt_reset() != ESP_OK) { fatal_ = true; state_ = "WATCHDOG_ERROR"; }
  }
  bool canUpdate() const { return !pending_ && !fatal_ && !contextError_ && updater.enabled(); }
  bool pending() const { return pending_; }
  const char* bootState() const { return state_; }
  const char* transaction() const { return transaction_; }
  const char* expectedBuild() const { return expected_.buildId; }
  Origin origin() const { return origin_; }
  uint32_t attemptVersion() const { return attemptVersion_ > writer.attemptVersion() ? attemptVersion_ : writer.attemptVersion(); }
  EspWriter writer;
  Updater updater;
 private:
  bool matches() const {
    return expected_.version == kOtaVersion && !strcmp(expected_.buildId, kBuildId) &&
      !strcmp(expected_.boardId, kBoardId) && expected_.minSchema <= kConfigSchema;
  }
  void rollback() {
    pending_ = false; fatal_ = true; state_ = "ROLLBACK_FAILED_USB_REQUIRED";
    // On success this does not return. Never erase settings or improvise a loop.
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
  Storage& storage_;
  Manifest expected_;
  char transaction_[33] = {};
  uint32_t started_ = 0;
  uint32_t healthyAt_ = 0, lastPoll_ = 0, attemptVersion_ = 0;
  Origin origin_ = Origin::LegacyPush;
  bool observing_ = false, contextError_ = false;
  bool pending_ = false, fatal_ = false;
  const char* state_ = "NOT_STARTED";
};
}}
