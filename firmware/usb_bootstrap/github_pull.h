#pragma once
#include "github_core.h"
#include "ota_runtime.h"
#include <string.h>

namespace bootstrap { namespace github {
enum class ResultCode { None, NoRelease, Unchanged, NoUpdate, Updated, Rejected, Network,
  Metadata, Time, RateLimit, Http, Cancelled, Resources };
inline const char* resultName(ResultCode value) {
  switch (value) {
    case ResultCode::NoRelease: return "NO_RELEASE";
    case ResultCode::Unchanged: return "UNCHANGED";
    case ResultCode::NoUpdate: return "NO_NEW_VERSION";
    case ResultCode::Updated: return "REBOOT_PENDING";
    case ResultCode::Rejected: return "UPDATE_REJECTED";
    case ResultCode::Network: return "TLS_OR_NETWORK_FAILED";
    case ResultCode::Metadata: return "METADATA_INVALID";
    case ResultCode::Time: return "TIME_UNAVAILABLE";
    case ResultCode::RateLimit: return "RATE_LIMITED";
    case ResultCode::Http: return "HTTP_REJECTED";
    case ResultCode::Cancelled: return "CANCELLED";
    case ResultCode::Resources: return "RESOURCE_LIMIT";
    default: return "NOT_CHECKED";
  }
}
struct CheckRequest { uint32_t floor = 0; unsigned failures = 0; char etag[kEtagMax] = {}; };
// Scalar diagnostics for the last TLS connection in a completed job. Never carry
// certificates, headers, signed redirect URLs or credentials across tasks.
struct TlsDiagnostics {
  bool attempted = false;
  int connectResult = 0;
  uint32_t connectMs = 0;
  int espError = 0, tlsError = 0, verifyFlags = 0;
};
enum class ExchangeFailure { None, QueueSend, NegativeAck, AckTimeout, Cancelled, JobTimeout, Resources };
struct Result {
  ResultCode code = ResultCode::None;
  unsigned httpStatus = 0;
  uint32_t waitMs = kPollMs, sampledMinHeap = 0, sampledMinBlock = 0, stackFreeBytes = 0;
  TlsDiagnostics tls;
  ExchangeFailure exchangeFailure = ExchangeFailure::None;
  uint32_t exchangeSequence = 0, exchangeWaitMs = 0, exchangeKind = 0;
  char etag[kEtagMax] = {};
  Release release;
};
enum class MessageKind { Header, Chunk, Done };
struct HealthSample {
  uint32_t heap = 0, block = 0, failed = 0;
};
struct Rejection {
  bool present = false;
  MessageKind kind = MessageKind::Done;
  uint32_t sequence = 0, received = 0, messageAge = 0, jobAge = 0, gates = 0;
  HealthSample health;
  char reason[32] = {};
};
struct Message {
  MessageKind kind = MessageKind::Done;
  uint32_t sequence = 0, created = 0;
  size_t size = 0;
  uint8_t bytes[ota::kChunkMax] = {};
  Result result;
};
// Only this interface crosses tasks. Worker owns TLS and bounded download
// buffers. The Arduino loop alone owns Runtime, NVS, flash and HTTP auth.
struct Port {
  virtual ~Port() = default;
  virtual bool start(const CheckRequest&) = 0;
  // On success sample the shared monotonic clock AFTER dequeue. A producer on
  // the other core can enqueue after the caller cached its loop timestamp.
  virtual bool take(Message&, uint32_t& receivedAt) = 0;
  virtual void reply(uint32_t sequence, bool accepted) = 0;
  virtual void cancel() = 0;
};
class Pull {
 public:
  Pull(Port& port, ota::Runtime& runtime, bool automatic) : port_(port), runtime_(runtime), automatic_(automatic) {}
  void begin(uint32_t now, uint32_t random) { schedule_.begin(now, random); }
  bool request(uint32_t now) {
    if (busy_ || requested_ || (startedOnce_ && !elapsed(now, started_, 60000)) ||
        (rateLimited_ && !schedule_.due(now))) return false;
    requested_ = true; return true;
  }
  bool busy() const { return busy_ || requested_; }
  bool automatic() const { return automatic_; }
  // RAM-only policy, owned by the loop. Never cancel an in-flight/manual job
  // or shorten an existing retry/backoff when toggled off and on.
  void setAutomatic(bool enabled, uint32_t now, uint32_t random) {
    if (enabled && !automatic_ && !startedOnce_) schedule_.begin(now, random);
    automatic_ = enabled;
  }
  bool rebootReady() const { return reboot_; }
  const Result& result() const { return result_; }
  const Rejection& rejection() const { return rejection_; }
  const char* state() const { return busy_ ? "CHECKING_OR_DOWNLOADING" : requested_ ? "QUEUED" : automatic_ ? "WAITING" : "AUTOMATIC_DISABLED"; }
  uint32_t nextMs(uint32_t now) const { return schedule_.remaining(now); }
  void poll(uint32_t now, bool healthy, uint32_t random, void (*randomToken)(char[33]), const HealthSample& health = HealthSample{}) {
    if (busy_) {
      if (elapsed(now, started_, kJobMs)) {
        port_.cancel();
        if (runtime_.updater.origin() == ota::Origin::GithubPull) runtime_.updater.interrupt();
      }
      Message message;
      if (!port_.take(message, now)) return;
      if (message.kind == MessageKind::Done) {
        result_ = message.result; busy_ = false;
        reboot_ = runtime_.updater.origin() == ota::Origin::GithubPull && runtime_.updater.phase() == ota::Phase::RebootPending;
        if (reboot_) result_.code = ResultCode::Updated;
        else if (runtime_.updater.origin() == ota::Origin::GithubPull) runtime_.updater.interrupt();
        const bool successful = result_.code == ResultCode::NoRelease || result_.code == ResultCode::Unchanged ||
          result_.code == ResultCode::NoUpdate || result_.code == ResultCode::Updated;
        failures_ = successful ? 0 : failures_ < 6 ? failures_ + 1 : 6;
        rateLimited_ = result_.code == ResultCode::RateLimit;
        if (successful && result_.etag[0]) memcpy(etag_, result_.etag, sizeof(etag_));
        else if (!successful) etag_[0] = 0; // Failed asset download must not be stranded behind a 304.
        schedule_.after(now, result_.waitMs);
        return;
      }
      bool accepted = false;
      const uint32_t gates = (elapsed(now, message.created, ota::kIdleMs) ? 1U : 0U) |
        (elapsed(now, started_, kJobMs) ? 2U : 0U) | (!healthy ? 4U : 0U) |
        (!runtime_.canUpdate() ? 8U : 0U);
      if (!gates) {
        if (message.kind == MessageKind::Header && message.size == ota::kPackageHeaderBytes) {
          ota::Manifest preview;
          char token[33]; randomToken(token);
          accepted = ota::decodePackageManifest(message.bytes, ota::kPackageManifestBytes, preview) &&
            preview.version == message.result.release.version && preview.imageSize + ota::kPackageHeaderBytes == message.result.release.size &&
            runtime_.updater.preparePackage(message.bytes, message.size, token, ota::Origin::GithubPull, now) &&
            runtime_.updater.start(token, preview.imageSize, now);
        } else if (message.kind == MessageKind::Chunk && runtime_.updater.origin() == ota::Origin::GithubPull) {
          accepted = runtime_.updater.chunk(message.bytes, message.size, now);
        }
      }
      if (!accepted && !rejection_.present) {
        rejection_.present = true; rejection_.kind = message.kind;
        rejection_.sequence = message.sequence; rejection_.received = runtime_.updater.received();
        rejection_.messageAge = now - message.created; rejection_.jobAge = now - started_;
        rejection_.gates = gates; rejection_.health = health;
        // Only updater-owned constant labels, never package or request content.
        strncpy(rejection_.reason, runtime_.updater.reason(), sizeof(rejection_.reason) - 1);
      }
      port_.reply(message.sequence, accepted);
      return;
    }
    const auto phase = runtime_.updater.phase();
    if (!healthy || !runtime_.canUpdate() || (phase != ota::Phase::Idle && phase != ota::Phase::Failed) || reboot_) return;
    if (!requested_ && (!automatic_ || !schedule_.due(now))) return;
    CheckRequest request; request.floor = kOtaVersion > runtime_.attemptVersion() ? kOtaVersion : runtime_.attemptVersion();
    request.failures = failures_; memcpy(request.etag, etag_, sizeof(etag_));
    requested_ = false; started_ = now; startedOnce_ = true;
    if (port_.start(request)) { rejection_ = Rejection{}; busy_ = true; }
    else { result_.code = ResultCode::Resources; schedule_.after(now, 60000); }
    (void)random; // The network worker owns the post-request jitter sample.
  }
 private:
  Port& port_;
  ota::Runtime& runtime_;
  bool automatic_;
  bool busy_ = false, requested_ = false, reboot_ = false, startedOnce_ = false, rateLimited_ = false;
  uint32_t started_ = 0;
  unsigned failures_ = 0;
  char etag_[kEtagMax] = {};
  Schedule schedule_;
  Result result_;
  Rejection rejection_;
};
}}
