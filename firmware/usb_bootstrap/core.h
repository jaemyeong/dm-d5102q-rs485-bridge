#pragma once

#include "config.h"

namespace bootstrap {
bool elapsed(uint32_t now, uint32_t since, uint32_t duration);
bool equalSecret(const char* a, const char* b);
uint32_t crc32(const uint8_t* data, size_t size);

struct WifiConfig {
  uint32_t revision = 0;
  char ssid[33] = {};
  char password[65] = {};
};
bool validWifi(const WifiConfig& config);
bool parseConfig(const char* json, size_t size, WifiConfig& config);
void encodeConfig(const WifiConfig& config, uint8_t out[kRecordBytes]);
bool decodeConfig(const uint8_t* data, size_t size, WifiConfig& config);

// read returns 0 only for a missing key; SIZE_MAX means an I/O/type/size error.
struct Storage {
  virtual ~Storage() = default;
  virtual size_t read(const char* key, uint8_t* data, size_t capacity) = 0;
  virtual bool write(const char* key, const uint8_t* data, size_t size) = 0;
  virtual bool erase(const char* key) = 0;
};

enum class ConfigState { Empty, Trial, Active, Corrupt };
class ConfigStore {
 public:
  explicit ConfigStore(Storage& storage) : storage_(storage) {}
  ConfigState load();
  bool stage(const WifiConfig& candidate);
  bool promote();
  bool discardTrial();
  bool resetWifi();
  // Must run before load: finish an interrupted, physically authorized reset.
  bool resumeReset();
  ConfigState state() const { return state_; }
  const WifiConfig& config() const { return config_; }
  uint32_t revision() const { return revision_; }
 private:
  bool checkedWrite(const char* key, const uint8_t* data, size_t size);
  Storage& storage_;
  ConfigState state_ = ConfigState::Corrupt;
  WifiConfig config_;
  uint32_t revision_ = 0;
  uint8_t selected_[kReferenceBytes] = {};
  uint8_t nextSlot_ = 0;
};

enum class Mode { Stopped, Provisioning, Trial, Station, Rebooting, Fault };
class BootResetGate {
 public:
  void begin(bool powerOn, bool pressed, uint32_t now) {
    waiting_ = powerOn && pressed; since_ = now;
  }
  bool tick(bool pressed, uint32_t now);
  bool waiting() const { return waiting_; }
 private:
  bool waiting_ = false;
  uint32_t since_ = 0;
};
enum class Action { None, StartAp, Connect, Promote, DiscardTrial, Reboot };
class NetworkState {
 public:
  Action begin(ConfigState config, uint32_t now);
  Action tick(uint32_t now, bool connected, uint32_t jitter);
  void promoted();
  void fault() { mode_ = Mode::Fault; }
  void saved(uint32_t now) { mode_ = Mode::Rebooting; since_ = now; }
  Mode mode() const { return mode_; }
 private:
  Mode mode_ = Mode::Stopped;
  uint32_t since_ = 0;
  uint32_t attempt_ = 0;
  uint32_t stable_ = 0;
  uint32_t retryMs_ = 5000;
  bool connected_ = false;
};

struct Request {
  char method[8] = {};
  char path[48] = {};
  char host[80] = {};
  char origin[96] = {};
  char csrf[33] = {};
  char authorization[768] = {};
  char contentType[48] = {};
  char uploadToken[33] = {};
  size_t contentLength = 0;
  bool hasLength = false;
};
bool parseHeaders(char* headers, Request& request);
bool allowedHost(const char* host, const char* localIp, const char* localHostname = "");
bool sameOrigin(const Request& request, const char* localIp, const char* csrf,
                const char* localHostname = "");
// Injectable standard SHA-256 implementation: lowercase 64-hex output.
using Hash = void (*)(const char*, char out[65]);
class DigestAuth {
 public:
  void begin(const char* key, const char* nonce, uint32_t now, Hash hash);
  bool expired(uint32_t now) const;
  bool verify(const Request& request, uint32_t now);
  const char* nonce() const { return nonce_; }
 private:
  struct Replay { char cnonce[65] = {}; uint32_t count = 0; } replay_[8];
  char ha1_[65] = {};
  char nonce_[33] = {};
  uint32_t created_ = 0;
  Hash hash_ = nullptr;
};
}
