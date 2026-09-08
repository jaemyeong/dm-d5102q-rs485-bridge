#include "github_core.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

namespace bootstrap { namespace github {
namespace {
bool decimal(const char* text, uint64_t& value) {
  value = 0;
  if (!text || !*text) return false;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9' || value > (UINT64_MAX - unsigned(*text - '0')) / 10) return false;
    value = value * 10 + unsigned(*text - '0');
  }
  return true;
}
bool copy(char* out, size_t capacity, const char* text) {
  const size_t size = strlen(text);
  if (size >= capacity) return false;
  memcpy(out, text, size + 1); return true;
}
bool identity(const char* text) {
  if (!*text) return false;
  for (; *text; ++text) if (!((*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z') ||
      (*text >= '0' && *text <= '9') || *text == '-' || *text == '_' || *text == '.')) return false;
  return true;
}
struct Json {
  const char* p;
  const char* end;
  unsigned depth = 0;
  Json(const char* begin, const char* finish) : p(begin), end(finish) {}
  void space() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
  bool take(char c) { space(); if (p == end || *p != c) return false; ++p; return true; }
  bool literal(const char* value) {
    space(); const size_t n = strlen(value);
    if (size_t(end - p) < n || memcmp(p, value, n)) return false;
    p += n; return true;
  }
  bool word(uint32_t& value) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
      if (p == end) return false;
      const char c = *p++;
      const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
        c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (digit < 0) return false;
      value = value * 16 + unsigned(digit);
    }
    return true;
  }
  bool string(char* out = nullptr, size_t capacity = 0) {
    if (!take('"')) return false;
    size_t used = 0;
    while (p < end) {
      uint32_t c = uint8_t(*p++);
      if (c == '"') { if (out) out[used] = 0; return true; }
      if (c < 32) return false;
      if (c == '\\') {
        if (p == end) return false;
        c = uint8_t(*p++);
        if (c == 'u') {
          if (!word(c)) return false;
          if (c >= 0xd800 && c <= 0xdbff) {
            uint32_t low;
            if (end - p < 2 || *p++ != '\\' || *p++ != 'u' || !word(low) || low < 0xdc00 || low > 0xdfff) return false;
            c = 0x10000;
          } else if (c >= 0xdc00 && c <= 0xdfff) return false;
        } else if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
        else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f';
        else if (c != '"' && c != '\\' && c != '/') return false;
      }
      // Needed metadata is deliberately ASCII; unknown strings are skipped.
      if (out) {
        if (c < 32 || c > 126 || used + 1 >= capacity) return false;
        out[used++] = char(c);
      }
    }
    return false;
  }
  bool number(uint64_t& value) {
    space(); const char* start = p; value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      if (value > (UINT64_MAX - unsigned(*p - '0')) / 10) return false;
      value = value * 10 + unsigned(*p++ - '0');
    }
    return p != start && (p - start == 1 || *start != '0');
  }
  bool skip() {
    space(); if (p == end || depth >= 8) return false;
    if (*p == '"') return string();
    if (*p == '{' || *p == '[') {
      const char close = *p++ == '{' ? '}' : ']'; ++depth;
      if (take(close)) { --depth; return true; }
      do { if (close == '}' && (!string() || !take(':'))) return false; if (!skip()) return false; } while (take(','));
      --depth; return take(close);
    }
    if (*p == 't') return literal("true");
    if (*p == 'f') return literal("false");
    if (*p == 'n') return literal("null");
    if (*p == '-') ++p;
    uint64_t value;
    if (!number(value)) return false;
    if (p < end && *p == '.') { ++p; const char* first = p; while (p < end && *p >= '0' && *p <= '9') ++p; if (p == first) return false; }
    if (p < end && (*p == 'e' || *p == 'E')) {
      ++p; if (p < end && (*p == '+' || *p == '-')) ++p;
      const char* first = p; while (p < end && *p >= '0' && *p <= '9') ++p; if (p == first) return false;
    }
    return true;
  }
};
bool asset(Json& json, Release& selected, bool& found) {
  if (!json.take('{')) return false;
  uint64_t id = 0, size = 0; char name[96] = {}, state[24] = {}; unsigned fields = 0;
  if (!json.take('}')) {
    do {
      char key[64]; if (!json.string(key, sizeof(key)) || !json.take(':')) return false;
      const unsigned bit = !strcmp(key, "id") ? 1 : !strcmp(key, "name") ? 2 : !strcmp(key, "size") ? 4 : !strcmp(key, "state") ? 8 : 0;
      if (bit && (fields & bit)) return false;
      fields |= bit;
      if (bit == 1) { if (!json.number(id)) return false; }
      else if (bit == 2) { if (!json.string(name, sizeof(name))) return false; }
      else if (bit == 4) { if (!json.number(size)) return false; }
      else if (bit == 8) { if (!json.string(state, sizeof(state))) return false; }
      else if (!json.skip()) return false;
    } while (json.take(','));
    if (!json.take('}')) return false;
  }
  constexpr char prefix[] = "dmbridge-atom-lite-";
  if (strncmp(name, prefix, sizeof(prefix) - 1)) return true;
  char* suffix = strstr(name + sizeof(prefix) - 1, ".dmota");
  if (!suffix || suffix[6]) return true;
  *suffix = 0; uint64_t version;
  if (found || fields != 15 || !id || strcmp(state, "uploaded") ||
      !decimal(name + sizeof(prefix) - 1, version) || !version || version > UINT32_MAX ||
      size < 224 || size > ota::kImageMax + ota::kPackageHeaderBytes) return false;
  selected.asset = id; selected.version = uint32_t(version); selected.size = uint32_t(size); found = true;
  return true;
}
}
bool parseRelease(const char* bytes, size_t size, Release& result) {
  result = Release{};
  if (!bytes || !size || size > kJsonMax || memchr(bytes, 0, size)) return false;
  Json json{bytes, bytes + size}; Release candidate; unsigned fields = 0; bool found = false;
  if (!json.take('{')) return false;
  do {
    char key[64]; if (!json.string(key, sizeof(key)) || !json.take(':')) return false;
    const unsigned bit = !strcmp(key, "id") ? 1 : !strcmp(key, "tag_name") ? 2 : !strcmp(key, "draft") ? 4 :
      !strcmp(key, "prerelease") ? 8 : !strcmp(key, "assets") ? 16 : 0;
    if (bit && (fields & bit)) return false;
    fields |= bit;
    if (bit == 1) { if (!json.number(candidate.id) || !candidate.id) return false; }
    else if (bit == 2) { if (!json.string(candidate.tag, sizeof(candidate.tag)) || !identity(candidate.tag)) return false; }
    else if (bit == 4 || bit == 8) { if (!json.literal("false")) return false; }
    else if (bit == 16) {
      if (!json.take('[')) return false;
      if (!json.take(']')) {
        do { if (!asset(json, candidate, found)) return false; } while (json.take(','));
        if (!json.take(']')) return false;
      }
    } else if (!json.skip()) return false;
  } while (json.take(','));
  if (!json.take('}')) return false;
  json.space();
  if (json.p != json.end || fields != 31 || !found) return false;
  result = candidate; return true;
}
bool parseUrl(const char* url, Url& result) {
  result = Url{};
  if (!url || strncmp(url, "https://", 8) || strlen(url) >= kUrlMax) return false;
  const char* path = strchr(url + 8, '/');
  if (!path || size_t(path - url - 8) >= sizeof(result.host)) return false;
  memcpy(result.host, url + 8, path - url - 8);
  if (strcmp(result.host, "api.github.com") && strcmp(result.host, "release-assets.githubusercontent.com")) return false;
  for (const char* p = path; *p; ++p) if (uint8_t(*p) < 33 || uint8_t(*p) > 126 || *p == '\\' || *p == '#') return false;
  return copy(result.path, sizeof(result.path), path);
}
bool parseHead(const char* bytes, size_t size, HttpHead& result) {
  result = HttpHead{};
  if (!bytes || size < 12 || size > kHttpHeadMax || memchr(bytes, 0, size) || memcmp(bytes + size - 4, "\r\n\r\n", 4)) return false;
  const char* p = bytes; const char* end = bytes + size; unsigned fields = 0;
  while (p < end) {
    const char* next = static_cast<const char*>(memchr(p, '\r', end - p));
    if (!next || next + 1 == end || next[1] != '\n' || size_t(next - p) >= kUrlMax + 64) return false;
    char line[kUrlMax + 64]; memcpy(line, p, next - p); line[next - p] = 0;
    for (const char* c = line; *c; ++c) if (uint8_t(*c) < 32 && *c != '\t') return false;
    if (p == bytes) {
      if (strncmp(line, "HTTP/1.1 ", 9) || strlen(line) < 12 ||
          line[9] < '1' || line[9] > '5' || line[10] < '0' || line[10] > '9' || line[11] < '0' || line[11] > '9' ||
          (line[12] && line[12] != ' ')) return false;
      result.status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
    } else if (*line) {
      char* colon = strchr(line, ':'); if (!colon || colon == line) return false;
      *colon++ = 0; while (*colon == ' ' || *colon == '\t') ++colon;
      const unsigned bit = !strcasecmp(line, "Content-Length") ? 1 : !strcasecmp(line, "Transfer-Encoding") ? 2 :
        !strcasecmp(line, "Location") ? 4 : !strcasecmp(line, "ETag") ? 8 : !strcasecmp(line, "Retry-After") ? 16 :
        !strcasecmp(line, "X-RateLimit-Reset") ? 32 : !strcasecmp(line, "X-RateLimit-Remaining") ? 64 :
        !strcasecmp(line, "Content-Encoding") ? 128 : !strcasecmp(line, "Content-Type") ? 256 : 0;
      if (bit && (fields & bit)) return false;
      fields |= bit; uint64_t number;
      if (bit == 1) { if (!decimal(colon, number) || number > UINT32_MAX) return false; result.length = uint32_t(number); result.hasLength = true; }
      else if (bit == 2) { if (strcasecmp(colon, "chunked")) return false; result.chunked = true; }
      else if (bit == 4) { if (!copy(result.location, sizeof(result.location), colon)) return false; }
      else if (bit == 8) {
        if (!copy(result.etag, sizeof(result.etag), colon)) return false;
        for (const char* c = colon; *c; ++c) if (uint8_t(*c) < 33 || uint8_t(*c) > 126) return false;
      } else if (bit == 16) { result.retrySeconds = decimal(colon, number) && number <= UINT32_MAX ? uint32_t(number) : UINT32_MAX; }
      else if (bit == 32) { if (!decimal(colon, result.rateReset)) return false; }
      else if (bit == 64) { if (!decimal(colon, number)) return false; result.remainingZero = number == 0; }
      else if (bit == 128 && strcasecmp(colon, "identity")) return false;
      else if (bit == 256) result.json = !strncasecmp(colon, "application/json", 16) && (!colon[16] || colon[16] == ';');
    } else if (next + 2 != end) return false;
    p = next + 2;
  }
  return !(result.hasLength && result.chunked);
}
uint32_t retryDelay(const HttpHead& head, uint64_t epoch, unsigned failures, uint32_t random) {
  if (head.status == 403 || head.status == 429) {
    uint64_t seconds = head.retrySeconds > 60 ? head.retrySeconds : 60;
    if (head.remainingZero && head.rateReset > epoch && head.rateReset - epoch > seconds) seconds = head.rateReset - epoch;
    const uint64_t backoff = 60ULL << (failures > 6 ? 6 : failures);
    if (backoff > seconds) seconds = backoff;
    return seconds > 2147480 ? UINT32_MAX : uint32_t(seconds * 1000 + random % 1000);
  }
  if (head.status == 200 || head.status == 304 || head.status == 404)
    return kPollMs - kPollJitterMs + random % (2 * kPollJitterMs + 1);
  const uint32_t wait = 60000U << (failures > 5 ? 5 : failures);
  return wait + random % 30001;
}
bool Body::feed(uint8_t byte, bool& payload) {
  payload = false;
  if (!valid_ || done_) return false;
  if (!chunked_ || phase_ == 1) {
    if (!left_ || received_ >= limit_) return valid_ = false;
    --left_; ++received_; payload = true;
    if (!left_) { if (chunked_) phase_ = 2; else done_ = true; }
    return true;
  }
  if (phase_ == 2) { if (byte != '\r') return valid_ = false; phase_ = 3; return true; }
  if (phase_ == 3) { if (byte != '\n') return valid_ = false; phase_ = 0; return true; }
  if (++trailerBytes_ > 2048 || lineUsed_ + 1 >= sizeof(line_)) return valid_ = false;
  line_[lineUsed_++] = char(byte);
  if (byte != '\n') return true;
  if (lineUsed_ < 2 || line_[lineUsed_ - 2] != '\r') return valid_ = false;
  line_[lineUsed_ - 2] = 0;
  if (phase_ == 4) { if (lineUsed_ == 2) done_ = true; }
  else {
    if (!*line_) return valid_ = false;
    left_ = 0;
    for (const char* c = line_; *c; ++c) {
      const int value = *c >= '0' && *c <= '9' ? *c - '0' : *c >= 'a' && *c <= 'f' ? *c - 'a' + 10 :
        *c >= 'A' && *c <= 'F' ? *c - 'A' + 10 : -1;
      if (value < 0 || left_ > (limit_ - received_) / 16 || left_ * 16 + unsigned(value) > limit_ - received_) return valid_ = false;
      left_ = left_ * 16 + unsigned(value);
    }
    phase_ = left_ ? 1 : 4; if (left_) trailerBytes_ = 0;
  }
  lineUsed_ = 0; return true;
}
}}
