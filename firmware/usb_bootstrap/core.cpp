#include "core.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace bootstrap {
namespace {
void put32(uint8_t* out, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}
uint32_t get32(const uint8_t* in) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(in[i]) << (8 * i);
  return value;
}
bool copy(char* out, size_t capacity, const char* in, size_t size) {
  if (size >= capacity) return false;
  memcpy(out, in, size);
  out[size] = 0;
  return true;
}
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool utf8(const char* text) {
  const auto* p = reinterpret_cast<const uint8_t*>(text);
  while (*p) {
    uint32_t cp = *p++;
    if (cp < 0x80) { if (cp < 0x20 || cp == 0x7f) return false; continue; }
    unsigned count = 0;
    uint32_t minimum = 0;
    if (cp >= 0xc2 && cp <= 0xdf) { cp &= 0x1f; count = 1; minimum = 0x80; }
    else if (cp >= 0xe0 && cp <= 0xef) { cp &= 0xf; count = 2; minimum = 0x800; }
    else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; count = 3; minimum = 0x10000; }
    else return false;
    while (count--) {
      if ((*p & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (*p++ & 0x3f);
    }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  }
  return true;
}
struct Json {
  const char* p;
  const char* end;
  void space() { while (p < end && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) ++p; }
  bool take(char c) { space(); if (p == end || *p != c) return false; ++p; return true; }
  bool number(uint32_t& out) {
    space();
    if (p == end || *p < '0' || *p > '9') return false;
    if (*p == '0' && p + 1 < end && p[1] >= '0' && p[1] <= '9') return false;
    out = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      const unsigned digit = static_cast<unsigned>(*p++ - '0');
      if (out > (UINT32_MAX - digit) / 10) return false;
      out = out * 10 + digit;
    }
    return true;
  }
  bool word(uint32_t& value) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
      if (p == end || hex(*p) < 0) return false;
      value = (value << 4) | static_cast<unsigned>(hex(*p++));
    }
    return true;
  }
  bool string(char* out, size_t capacity) {
    if (!take('"')) return false;
    size_t used = 0;
    while (p < end) {
      uint32_t c = static_cast<uint8_t>(*p++);
      if (c == '"') { out[used] = 0; return true; }
      if (c < 0x20) return false;
      if (c == '\\') {
        if (p == end) return false;
        c = static_cast<uint8_t>(*p++);
        if (c == 'u') {
          if (!word(c)) return false;
          if (c >= 0xd800 && c <= 0xdbff) {
            uint32_t low;
            if (end - p < 2 || *p++ != '\\' || *p++ != 'u' || !word(low) || low < 0xdc00 || low > 0xdfff) return false;
            c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
          } else if (c >= 0xdc00 && c <= 0xdfff) return false;
          if (c == 0) return false;
          const size_t bytes = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
          if (used + bytes >= capacity) return false;
          if (bytes == 1) out[used++] = static_cast<char>(c);
          else {
            out[used++] = static_cast<char>((bytes == 2 ? 0xc0 : bytes == 3 ? 0xe0 : 0xf0) | (c >> (6 * (bytes - 1))));
            for (size_t n = bytes - 1; n > 0; --n) out[used++] = static_cast<char>(0x80 | ((c >> (6 * (n - 1))) & 0x3f));
          }
          continue;
        }
        if (c != '"' && c != '\\' && c != '/') return false;
      }
      if (used + 1 >= capacity) return false;
      out[used++] = static_cast<char>(c);
    }
    return false;
  }
};
void reference(uint8_t slot, const WifiConfig& config, uint8_t out[kReferenceBytes]) {
  out[0] = slot;
  put32(out + 1, config.revision);
  uint8_t record[kRecordBytes];
  encodeConfig(config, record);
  memcpy(out + 5, record + 108, 4);
  put32(out + 9, crc32(out, 9));
}
bool validReference(const uint8_t* ref, size_t size, const WifiConfig configs[2], const bool valid[2]) {
  if (size != kReferenceBytes || ref[0] > 1 || !valid[ref[0]]) return false;
  uint8_t expected[kReferenceBytes];
  reference(ref[0], configs[ref[0]], expected);
  return memcmp(ref, expected, sizeof(expected)) == 0;
}
}

bool elapsed(uint32_t now, uint32_t since, uint32_t duration) { return uint32_t(now - since) >= duration; }
bool equalSecret(const char* a, const char* b) {
  const size_t length = strlen(a);
  if (strlen(b) != length) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < length; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}
uint32_t crc32(const uint8_t* data, size_t size) {
  uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0U);
  }
  return ~crc;
}
bool validWifi(const WifiConfig& config) {
  const size_t ssid = strnlen(config.ssid, sizeof(config.ssid));
  const size_t password = strnlen(config.password, sizeof(config.password));
  if (ssid == 0 || ssid > 32 || password < 8 || password > 64 || !utf8(config.ssid)) return false;
  for (size_t i = 0; i < password; ++i) {
    const auto c = static_cast<uint8_t>(config.password[i]);
    if (c < 0x20 || c > 0x7e || (password == 64 && hex(config.password[i]) < 0)) return false;
  }
  return true;
}
bool parseConfig(const char* json, size_t size, WifiConfig& config) {
  if (size == 0 || size > kBodyMax || memchr(json, 0, size)) return false;
  Json parser{json, json + size};
  WifiConfig candidate;
  unsigned fields = 0;
  if (!parser.take('{')) return false;
  for (unsigned i = 0; i < 3; ++i) {
    char key[24];
    if ((i && !parser.take(',')) || !parser.string(key, sizeof(key)) || !parser.take(':')) return false;
    unsigned bit;
    if (strcmp(key, "ssid") == 0) {
      bit = 1;
      if (!parser.string(candidate.ssid, sizeof(candidate.ssid))) return false;
    } else if (strcmp(key, "password") == 0) {
      bit = 2;
      if (!parser.string(candidate.password, sizeof(candidate.password))) return false;
    } else if (strcmp(key, "configRevision") == 0) {
      bit = 4;
      if (!parser.number(candidate.revision)) return false;
    } else return false;
    if (fields & bit) return false;
    fields |= bit;
  }
  if (!parser.take('}')) return false;
  parser.space();
  if (fields != 7 || parser.p != parser.end || !validWifi(candidate)) return false;
  config = candidate;
  return true;
}
void encodeConfig(const WifiConfig& config, uint8_t out[kRecordBytes]) {
  memset(out, 0, kRecordBytes);
  memcpy(out, "DMB0", 4);
  out[4] = 1;
  put32(out + 6, config.revision);
  out[10] = static_cast<uint8_t>(strlen(config.ssid));
  out[11] = static_cast<uint8_t>(strlen(config.password));
  memcpy(out + 12, config.ssid, out[10]);
  memcpy(out + 44, config.password, out[11]);
  put32(out + 108, crc32(out, 108));
}
bool decodeConfig(const uint8_t* data, size_t size, WifiConfig& config) {
  if (size != kRecordBytes || memcmp(data, "DMB0", 4) || data[4] != 1 || data[5] != 0 ||
      data[10] == 0 || data[10] > 32 || data[11] > 64 || get32(data + 108) != crc32(data, 108)) return false;
  WifiConfig candidate;
  candidate.revision = get32(data + 6);
  memcpy(candidate.ssid, data + 12, data[10]);
  memcpy(candidate.password, data + 44, data[11]);
  if (!candidate.revision || strlen(candidate.ssid) != data[10] || strlen(candidate.password) != data[11] || !validWifi(candidate)) return false;
  uint8_t canonical[kRecordBytes];
  encodeConfig(candidate, canonical);
  if (memcmp(data, canonical, kRecordBytes)) return false;
  config = candidate;
  return true;
}
bool ConfigStore::checkedWrite(const char* key, const uint8_t* data, size_t size) {
  uint8_t check[kRecordBytes];
  return storage_.write(key, data, size) && storage_.read(key, check, sizeof(check)) == size && memcmp(data, check, size) == 0;
}
ConfigState ConfigStore::load() {
  state_ = ConfigState::Corrupt;
  config_ = WifiConfig{};
  revision_ = 0;
  nextSlot_ = 0;
  WifiConfig configs[2];
  bool valid[2] = {};
  bool damaged = false;
  for (unsigned i = 0; i < 2; ++i) {
    uint8_t bytes[kRecordBytes];
    const size_t size = storage_.read(i ? "cfgB" : "cfgA", bytes, sizeof(bytes));
    if (size == SIZE_MAX) return state_;
    valid[i] = decodeConfig(bytes, size, configs[i]);
    damaged |= size != 0 && !valid[i];
    if (valid[i] && configs[i].revision >= revision_) { revision_ = configs[i].revision; nextSlot_ = 1 - i; }
  }
  uint8_t active[kReferenceBytes], trial[kReferenceBytes];
  const size_t activeSize = storage_.read("active", active, sizeof(active));
  const size_t trialSize = storage_.read("trial", trial, sizeof(trial));
  if (activeSize == SIZE_MAX || trialSize == SIZE_MAX) return state_;
  if (activeSize) {
    if (!validReference(active, activeSize, configs, valid)) return state_;
    // A stale trial left by power loss after promotion cannot override active.
    if (trialSize && (trialSize != activeSize || memcmp(active, trial, activeSize))) return state_;
    memcpy(selected_, active, sizeof(selected_));
    config_ = configs[active[0]];
    state_ = ConfigState::Active;
  } else if (trialSize) {
    if (!validReference(trial, trialSize, configs, valid)) return state_;
    memcpy(selected_, trial, sizeof(selected_));
    config_ = configs[trial[0]];
    state_ = ConfigState::Trial;
  } else if (!damaged) state_ = ConfigState::Empty;
  return state_;
}
bool ConfigStore::stage(const WifiConfig& candidate) {
  if (state_ != ConfigState::Empty || candidate.revision != revision_ || revision_ == UINT32_MAX || !validWifi(candidate)) return false;
  WifiConfig next = candidate;
  ++next.revision;
  uint8_t bytes[kRecordBytes];
  encodeConfig(next, bytes);
  reference(nextSlot_, next, selected_);
  if (!checkedWrite(nextSlot_ ? "cfgB" : "cfgA", bytes, sizeof(bytes)) ||
      !checkedWrite("trial", selected_, sizeof(selected_))) { state_ = ConfigState::Corrupt; return false; }
  config_ = next;
  revision_ = next.revision;
  state_ = ConfigState::Trial;
  return true;
}
bool ConfigStore::promote() {
  if (state_ != ConfigState::Trial || !checkedWrite("active", selected_, sizeof(selected_))) { state_ = ConfigState::Corrupt; return false; }
  // If erasing a stale identical trial fails, active is still authoritative.
  storage_.erase("trial");
  state_ = ConfigState::Active;
  return true;
}
bool ConfigStore::discardTrial() {
  uint8_t check[kReferenceBytes];
  if (state_ != ConfigState::Trial || !storage_.erase("trial") || storage_.read("trial", check, sizeof(check)) != 0) {
    state_ = ConfigState::Corrupt;
    return false;
  }
  config_ = WifiConfig{};
  nextSlot_ = 1 - selected_[0];
  state_ = ConfigState::Empty;
  return true;
}
Action NetworkState::begin(ConfigState config, uint32_t now) {
  since_ = attempt_ = now;
  connected_ = false;
  retryMs_ = 5000;
  if (config == ConfigState::Corrupt) { mode_ = Mode::Fault; return Action::None; }
  if (config == ConfigState::Empty) { mode_ = Mode::Provisioning; return Action::StartAp; }
  mode_ = config == ConfigState::Trial ? Mode::Trial : Mode::Station;
  return Action::Connect;
}
Action NetworkState::tick(uint32_t now, bool connected, uint32_t jitter) {
  if (mode_ == Mode::Rebooting && elapsed(now, since_, kRebootMs)) return Action::Reboot;
  if (mode_ != Mode::Trial && mode_ != Mode::Station) return Action::None;
  if (connected && !connected_) stable_ = now;
  if (!connected && connected_) { attempt_ = now; retryMs_ = 5000; }
  connected_ = connected;
  if (mode_ == Mode::Trial) {
    if (connected && elapsed(now, stable_, kStableMs)) return Action::Promote;
    if (elapsed(now, since_, kTrialMs)) return Action::DiscardTrial;
  }
  if (!connected && elapsed(now, attempt_, retryMs_)) {
    attempt_ = now;
    retryMs_ = retryMs_ >= 30000 ? 60000 : retryMs_ * 2;
    retryMs_ += jitter % 1001;
    if (retryMs_ > 60000) retryMs_ = 60000;
    return Action::Connect;
  }
  return Action::None;
}
void NetworkState::promoted() { mode_ = Mode::Station; }

bool parseHeaders(char* headers, Request& request) {
  request = Request{};
  char* line = strstr(headers, "\r\n");
  if (!line) return false;
  *line = 0;
  char* path = strchr(headers, ' ');
  if (!path) return false;
  *path++ = 0;
  char* version = strchr(path, ' ');
  if (!version) return false;
  *version++ = 0;
  if (strcmp(version, "HTTP/1.1") || !copy(request.method, sizeof(request.method), headers, strlen(headers)) ||
      !copy(request.path, sizeof(request.path), path, strlen(path)) || path[0] != '/') return false;
  for (const char* p = path; *p; ++p) if (*p <= ' ' || *p >= 0x7f) return false;
  uint32_t seen = 0;
  char* p = line + 2;
  while (*p) {
    line = strstr(p, "\r\n");
    if (!line) return false;
    *line = 0;
    if (*p == 0) return line[2] == 0 && (seen & 1);
    char* value = strchr(p, ':');
    if (!value || value == p) return false;
    *value++ = 0;
    for (char* c = p; *c; ++c) {
      if (!isalnum(static_cast<unsigned char>(*c)) && *c != '-') return false;
      *c = static_cast<char>(tolower(static_cast<unsigned char>(*c)));
    }
    while (*value == ' ' || *value == '\t') ++value;
    char* end = value + strlen(value);
    while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
    for (const char* c = value; *c; ++c) if (static_cast<uint8_t>(*c) < 0x20 || static_cast<uint8_t>(*c) >= 0x7f) return false;
    unsigned bit = 0;
    char* dest = nullptr;
    size_t capacity = 0;
    if (!strcmp(p, "host")) { bit = 1; dest = request.host; capacity = sizeof(request.host); }
    else if (!strcmp(p, "origin")) { bit = 2; dest = request.origin; capacity = sizeof(request.origin); }
    else if (!strcmp(p, "x-csrf-token")) { bit = 4; dest = request.csrf; capacity = sizeof(request.csrf); }
    else if (!strcmp(p, "authorization")) { bit = 8; dest = request.authorization; capacity = sizeof(request.authorization); }
    else if (!strcmp(p, "content-type")) { bit = 16; dest = request.contentType; capacity = sizeof(request.contentType); }
    else if (!strcmp(p, "x-ota-token")) { bit = 64; dest = request.uploadToken; capacity = sizeof(request.uploadToken); }
    else if (!strcmp(p, "content-length")) {
      bit = 32;
      request.hasLength = true;
      if (!*value) return false;
      for (const char* c = value; *c; ++c) {
        if (*c < '0' || *c > '9') return false;
        request.contentLength = request.contentLength * 10 + static_cast<unsigned>(*c - '0');
        const size_t limit = !strcmp(request.path, "/api/v1/ota/upload") ? kUploadMax : kBodyMax;
        if (request.contentLength > limit) return false;
      }
    } else if (!strcmp(p, "transfer-encoding") || !strcmp(p, "expect")) return false;
    if ((seen & bit) || (dest && !copy(dest, capacity, value, strlen(value)))) return false;
    seen |= bit;
    p = line + 2;
  }
  return false;
}
namespace {
bool normalizedHost(const char* value, char out[80]) {
  size_t length = strlen(value);
  if (length > 3 && !strcmp(value + length - 3, ":80")) length -= 3;
  if (!length || length >= 80) return false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    out[i] = static_cast<char>(tolower(c));
  }
  out[length] = 0;
  return true;
}
}
bool allowedHost(const char* host, const char* localIp, const char* localHostname) {
  char normalized[80];
  if (!normalizedHost(host, normalized)) return false;
  return !strcmp(normalized, localIp) || (*localHostname && !strcmp(normalized, localHostname));
}
bool sameOrigin(const Request& request, const char* localIp, const char* csrf, const char* localHostname) {
  if (!allowedHost(request.host, localIp, localHostname) || strncmp(request.origin, "http://", 7)) return false;
  char host[80], origin[80];
  return normalizedHost(request.host, host) && normalizedHost(request.origin + 7, origin) &&
    !strcmp(host, origin) && strlen(csrf) == 32 && equalSecret(request.csrf, csrf);
}

bool BootResetGate::tick(bool pressed, uint32_t now) {
  if (!waiting_) return false;
  // Any release cancels this boot's reset, including bounce: safe false negative.
  if (!pressed) { waiting_ = false; return false; }
  if (!elapsed(now, since_, kBootResetMs)) return false;
  waiting_ = false;
  return true;
}

bool ConfigStore::resetWifi() {
  // Durable intent precedes all deletion. Never erase the namespace/enroll key.
  const uint8_t marker[] = {'W', 'R', 'S', 'T', 1, 0x6d, 0xa3, 0x52};
  state_ = ConfigState::Corrupt;
  return checkedWrite("reset", marker, sizeof(marker)) && resumeReset();
}
bool ConfigStore::resumeReset() {
  const uint8_t marker[] = {'W', 'R', 'S', 'T', 1, 0x6d, 0xa3, 0x52};
  uint8_t bytes[kRecordBytes] = {};
  const size_t size = storage_.read("reset", bytes, sizeof(bytes));
  if (!size) return true;
  state_ = ConfigState::Corrupt;
  if (size != sizeof(marker) || memcmp(bytes, marker, sizeof(marker))) return false;
  const char* keys[] = {"active", "trial", "cfgA", "cfgB", "ghauto", "reset"};
  for (const auto key : keys) {
    if (!storage_.erase(key) || storage_.read(key, bytes, sizeof(bytes)) != 0) return false;
  }
  config_ = WifiConfig{};
  revision_ = nextSlot_ = 0;
  memset(selected_, 0, sizeof(selected_));
  state_ = ConfigState::Empty;
  return true;
}

void DigestAuth::begin(const char* key, const char* nonce, uint32_t now, Hash hash) {
  char input[128];
  snprintf(input, sizeof(input), "%s:%s:%s", kUsername, kRealm, key);
  hash(input, ha1_);
  copy(nonce_, sizeof(nonce_), nonce, strlen(nonce));
  created_ = now;
  hash_ = hash;
  for (auto& entry : replay_) entry = Replay{};
}
bool DigestAuth::expired(uint32_t now) const { return !hash_ || elapsed(now, created_, kNonceMs); }
bool DigestAuth::verify(const Request& request, uint32_t now) {
  if (expired(now) || strlen(ha1_) != 64 || strncmp(request.authorization, "Digest ", 7)) return false;
  const char* p = request.authorization + 7;
  const char* names[] = {"username", "realm", "nonce", "uri", "response", "algorithm", "qop", "nc", "cnonce"};
  char values[9][65] = {};
  unsigned seen = 0;
  while (*p) {
    while (*p == ' ') ++p;
    const char* start = p;
    while (isalnum(static_cast<unsigned char>(*p)) || *p == '-') ++p;
    char name[24];
    if (!copy(name, sizeof(name), start, static_cast<size_t>(p - start))) return false;
    while (*p == ' ') ++p;
    if (*p++ != '=') return false;
    while (*p == ' ') ++p;
    const bool quoted = *p == '"';
    if (quoted) ++p;
    start = p;
    while (*p && (quoted ? *p != '"' : *p != ',' && *p != ' ')) {
      if (*p == '\\' || *p < 0x20 || *p >= 0x7f) return false;
      ++p;
    }
    const size_t length = static_cast<size_t>(p - start);
    if (quoted && *p++ != '"') return false;
    unsigned index = 0;
    while (index < 9 && strcmp(name, names[index])) ++index;
    if (index == 9 || (seen & (1U << index)) || !copy(values[index], sizeof(values[index]), start, length)) return false;
    seen |= 1U << index;
    while (*p == ' ') ++p;
    if (!*p) break;
    if (*p++ != ',' || !*p) return false;
  }
  if (seen != 511 || strcmp(values[0], kUsername) || strcmp(values[1], kRealm) ||
      !equalSecret(values[2], nonce_) || strcmp(values[3], request.path) ||
      strcmp(values[5], "SHA-256") || strcmp(values[6], "auth") || strlen(values[7]) != 8 || !*values[8]) return false;
  uint32_t count = 0;
  for (const char* c = values[7]; *c; ++c) { if (hex(*c) < 0) return false; count = (count << 4) | static_cast<unsigned>(hex(*c)); }
  if (!count) return false;
  Replay* slot = nullptr;
  for (auto& entry : replay_) {
    if (!strcmp(entry.cnonce, values[8])) { slot = &entry; break; }
    if (!entry.count && !slot) slot = &entry;
  }
  if (!slot || count <= slot->count) return false;
  char input[384], ha2[65], expected[65];
  snprintf(input, sizeof(input), "%s:%s", request.method, request.path);
  hash_(input, ha2);
  snprintf(input, sizeof(input), "%s:%s:%s:%s:auth:%s", ha1_, nonce_, values[7], values[8], ha2);
  hash_(input, expected);
  if (strlen(expected) != 64 || !equalSecret(expected, values[4])) return false;
  copy(slot->cnonce, sizeof(slot->cnonce), values[8], strlen(values[8]));
  slot->count = count;
  return true;
}
}
