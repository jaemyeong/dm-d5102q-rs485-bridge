#pragma once
#include "ota_core.h"

namespace bootstrap { namespace github {
constexpr size_t kJsonMax = 16384, kHttpHeadMax = 8192, kUrlMax = 1536, kEtagMax = 128;
constexpr uint32_t kPollMs = 300000, kPollJitterMs = 30000, kJobMs = 180000;
constexpr char kRepositoryPath[] = "/repos/jaemyeong/dm-d5102q-rs485-bridge";
constexpr char kLatestPath[] = "/repos/jaemyeong/dm-d5102q-rs485-bridge/releases/latest";
constexpr char kApiVersion[] = "2026-03-10";
struct Release {
  uint64_t id = 0, asset = 0;
  uint32_t version = 0, size = 0;
  char tag[64] = {};
};
bool parseRelease(const char* json, size_t size, Release& result);
struct Url { char host[64] = {}, path[kUrlMax] = {}; };
bool parseUrl(const char* url, Url& result);
struct HttpHead {
  unsigned status = 0;
  bool hasLength = false, chunked = false, json = false, remainingZero = false;
  uint32_t length = 0, retrySeconds = 0;
  uint64_t rateReset = 0;
  char location[kUrlMax] = {}, etag[kEtagMax] = {};
};
bool parseHead(const char* bytes, size_t size, HttpHead& result);
uint32_t retryDelay(const HttpHead& head, uint64_t epoch, unsigned failures, uint32_t random);
class Schedule {
 public:
  void begin(uint32_t now, uint32_t random) { at_ = now; wait_ = 30000 + random % 30001; }
  bool due(uint32_t now) const { return wait_ != UINT32_MAX && elapsed(now, at_, wait_); }
  uint32_t remaining(uint32_t now) const { return due(now) ? 0 : wait_ == UINT32_MAX ? wait_ : wait_ - uint32_t(now - at_); }
  void after(uint32_t now, uint32_t wait) { at_ = now; wait_ = wait; }
 private:
  uint32_t at_ = 0, wait_ = UINT32_MAX;
};
// Decodes framing without allocating a body. The caller must observe complete()
// BEFORE releasing the last image chunk to the OTA writer.
class Body {
 public:
  Body(const HttpHead& head, uint32_t limit) : left_(head.length), limit_(limit), chunked_(head.chunked) {
    valid_ = chunked_ || (head.hasLength && head.length <= limit);
    done_ = valid_ && !chunked_ && !left_;
  }
  bool feed(uint8_t byte, bool& payload);
  bool complete() const { return done_; }
  bool valid() const { return valid_; }
 private:
  uint32_t left_, limit_, received_ = 0;
  bool chunked_, done_ = false, valid_ = false;
  unsigned phase_ = 0, lineUsed_ = 0, trailerBytes_ = 0;
  char line_[128] = {};
};
}}
