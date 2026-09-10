#include "core.h"
#include "automatic_policy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

using namespace bootstrap;
namespace {
unsigned checks = 0;
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while (false)
class FakeStorage : public Storage {
 public:
  std::map<std::string, std::vector<uint8_t>> data;
  int operations = 0, failAt = -1;
  bool persistedFailure = false;
  bool fail() { return ++operations == failAt; }
  size_t read(const char* key, uint8_t* out, size_t capacity) override {
    if (fail()) return SIZE_MAX;
    if (!data.count(key)) return 0;
    const auto& bytes = data.at(key);
    if (bytes.size() > capacity || bytes.empty()) return SIZE_MAX;
    memcpy(out, bytes.data(), bytes.size());
    return bytes.size();
  }
  bool write(const char* key, const uint8_t* bytes, size_t size) override {
    const bool failed = fail();
    if (!failed || persistedFailure) data[key] = std::vector<uint8_t>(bytes, bytes + size);
    return !failed;
  }
  bool erase(const char* key) override {
    const bool failed = fail();
    if (!failed || persistedFailure) data.erase(key);
    return !failed;
  }
};
WifiConfig wifi(uint32_t revision = 0) {
  WifiConfig c;
  c.revision = revision;
  strcpy(c.ssid, "test-net");
  strcpy(c.password, "test-only-password");
  return c;
}
void hash(const char* input, char out[65]) {
  uint8_t bytes[32];
#ifdef __APPLE__
  CC_SHA256(input, static_cast<CC_LONG>(strlen(input)), bytes);
#else
  SHA256(reinterpret_cast<const uint8_t*>(input), strlen(input), bytes);
#endif
  for (unsigned i = 0; i < 32; ++i) snprintf(out + i * 2, 3, "%02x", bytes[i]);
}
Request signedRequest(const char* nonce, uint32_t count, const char* cnonce = "test-client") {
  Request r;
  strcpy(r.method, "PUT"); strcpy(r.path, "/api/v1/config");
  char input[384], ha1[65], ha2[65], response[65];
  hash("installer:DM-BRIDGE-USB:test-only-key", ha1);
  hash("PUT:/api/v1/config", ha2);
  snprintf(input, sizeof(input), "%s:%s:%08x:%s:auth:%s", ha1, nonce, count, cnonce, ha2);
  hash(input, response);
  snprintf(r.authorization, sizeof(r.authorization),
    "Digest username=\"installer\", realm=\"DM-BRIDGE-USB\", nonce=\"%s\", uri=\"/api/v1/config\", "
    "response=\"%s\", algorithm=SHA-256, qop=auth, nc=%08x, cnonce=\"%s\"", nonce, response, count, cnonce);
  return r;
}
void testJsonAndRecords() {
  WifiConfig c;
  const std::string valid = "{\"ssid\":\"net-\\uD83D\\uDE00\",\"password\":\"12345678\",\"configRevision\":0}";
  CHECK(parseConfig(valid.data(), valid.size(), c));
  CHECK(std::string(c.ssid) == "net-\xf0\x9f\x98\x80");
  const char* invalid[] = {
    "{}", "[]", "null", "{\"ssid\":\"x\",\"ssid\":\"y\",\"password\":\"12345678\"}",
    "{\"ssid\":\"x\",\"password\":\"short\",\"configRevision\":0}",
    "{\"ssid\":\"x\",\"password\":\"12345678\",\"configRevision\":-1}",
    "{\"ssid\":\"x\",\"password\":\"12345678\",\"configRevision\":4294967296}",
    "{\"ssid\":\"x\",\"password\":\"12345678\",\"configRevision\":00}",
    "{\"ssid\":\"x\",\"password\":\"12345678\",\"configRevision\":0} trailing",
    "{\"ssid\":\"\\u0000\",\"password\":\"12345678\",\"configRevision\":0}",
    "{\"ssid\":\"\\uD800\",\"password\":\"12345678\",\"configRevision\":0}",
    "{\"ssid\":\"\\uDC00\",\"password\":\"12345678\",\"configRevision\":0}",
    "{\"ssid\":\"\xc0\xaf\",\"password\":\"12345678\",\"configRevision\":0}",
    "{\"ssid\":\"x\",\"password\":\"12345678\",\"configRevision\":0,\"extra\":1}"
  };
  for (const char* input : invalid) CHECK(!parseConfig(input, strlen(input), c));
  for (size_t size = 0; size < valid.size(); ++size) CHECK(!parseConfig(valid.data(), size, c));
  c = wifi(1);
  memset(c.ssid, 'a', 32); c.ssid[32] = 0;
  memset(c.password, 'a', 64); c.password[64] = 0;
  CHECK(validWifi(c));
  c.password[0] = 'z'; CHECK(!validWifi(c));
  c = wifi(1);
  uint8_t record[kRecordBytes]; encodeConfig(c, record);
  WifiConfig decoded;
  CHECK(decodeConfig(record, sizeof(record), decoded));
  CHECK(decoded.revision == 1 && !strcmp(decoded.ssid, c.ssid));
  for (size_t i = 0; i < sizeof(record); ++i) {
    record[i] ^= 1; CHECK(!decodeConfig(record, sizeof(record), decoded)); record[i] ^= 1;
  }
  CHECK(crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926);
}
void testStorage() {
  FakeStorage storage;
  ConfigStore store(storage);
  CHECK(store.load() == ConfigState::Empty);
  CHECK(!store.stage(wifi(1)));
  CHECK(store.stage(wifi()));
  CHECK(store.state() == ConfigState::Trial && store.revision() == 1);
  ConfigStore reboot(storage);
  CHECK(reboot.load() == ConfigState::Trial);
  CHECK(reboot.promote());
  CHECK(reboot.load() == ConfigState::Active);
  CHECK(!reboot.stage(wifi(1)));
  storage.data["cfgA"][20] ^= 1;
  CHECK(reboot.load() == ConfigState::Corrupt);
  for (int failAt = 1; failAt <= 4; ++failAt) {
    FakeStorage fake;
    ConfigStore candidate(fake);
    CHECK(candidate.load() == ConfigState::Empty);
    fake.operations = 0; fake.failAt = failAt;
    CHECK(!candidate.stage(wifi()));
    CHECK(candidate.state() == ConfigState::Corrupt);
    fake.failAt = -1;
    ConfigStore afterPowerLoss(fake);
    const auto state = afterPowerLoss.load();
    // A commit may persist even if its readback failed: only a trial is allowed.
    CHECK(state == ConfigState::Empty || state == ConfigState::Trial);
    CHECK(state != ConfigState::Active);
  }
  for (int failAt : {1, 3}) {
    FakeStorage uncertain;
    ConfigStore interrupted(uncertain);
    CHECK(interrupted.load() == ConfigState::Empty);
    uncertain.operations = 0; uncertain.failAt = failAt; uncertain.persistedFailure = true;
    CHECK(!interrupted.stage(wifi()));
    uncertain.failAt = -1;
    const auto recovered = interrupted.load();
    CHECK(recovered == (failAt == 1 ? ConfigState::Empty : ConfigState::Trial));
  }
  FakeStorage trial;
  ConfigStore recover(trial);
  CHECK(recover.load() == ConfigState::Empty && recover.stage(wifi()));
  CHECK(recover.discardTrial());
  CHECK(recover.load() == ConfigState::Empty && recover.revision() == 1);
  CHECK(recover.stage(wifi(1)) && recover.load() == ConfigState::Trial);
  CHECK(recover.config().revision == 2);
  trial.operations = 0; trial.failAt = 3; // erase stale trial after durable promotion
  CHECK(recover.promote());
  trial.failAt = -1;
  CHECK(recover.load() == ConfigState::Active);
  for (int failure = 1; failure <= 4; ++failure) {
    FakeStorage broken = trial;
    broken.operations = 0; broken.failAt = failure;
    ConfigStore unreadable(broken);
    CHECK(unreadable.load() == ConfigState::Corrupt);
  }
  FakeStorage uncommitted;
  uint8_t bytes[kRecordBytes]; encodeConfig(wifi(8), bytes);
  uncommitted.data["cfgA"] = std::vector<uint8_t>(bytes, bytes + sizeof(bytes));
  ConfigStore ignored(uncommitted);
  CHECK(ignored.load() == ConfigState::Empty && ignored.revision() == 8);
}
void testNetwork() {
  NetworkState machine;
  const uint32_t start = UINT32_MAX - 1000;
  CHECK(machine.begin(ConfigState::Empty, start) == Action::StartAp);
  for (uint32_t delta : {599999u, 600000u, 600001u, 86400000u, UINT32_MAX}) {
    CHECK(machine.tick(start + delta, false, 0) == Action::None);
    CHECK(machine.mode() == Mode::Provisioning);
  }
  CHECK(machine.begin(ConfigState::Trial, start) == Action::Connect);
  CHECK(machine.tick(start + 5000, false, 0) == Action::Connect);
  CHECK(machine.tick(start + 6000, true, 0) == Action::None);
  CHECK(machine.tick(start + 10999, true, 0) == Action::None);
  CHECK(machine.tick(start + 11000, true, 0) == Action::Promote);
  machine.promoted();
  CHECK(machine.mode() == Mode::Station);
  CHECK(machine.tick(start + 12000, false, 0) == Action::None);
  CHECK(machine.tick(start + 17000, false, 0) == Action::Connect);
  CHECK(machine.tick(start + 1200000u, false, 1000) != Action::StartAp);
  CHECK(machine.begin(ConfigState::Trial, 0) == Action::Connect);
  CHECK(machine.tick(kTrialMs, false, 0) == Action::DiscardTrial);
  machine.saved(start);
  CHECK(machine.tick(start + kRebootMs - 1, false, 0) == Action::None);
  CHECK(machine.tick(start + kRebootMs, false, 0) == Action::Reboot);
  CHECK(machine.begin(ConfigState::Corrupt, 0) == Action::None);
  CHECK(machine.mode() == Mode::Fault);
}
void testHeadersAndAuth() {
  auto parse = [](const std::string& input, Request& r) {
    std::vector<char> bytes(input.begin(), input.end()); bytes.push_back(0);
    return parseHeaders(bytes.data(), r);
  };
  Request r;
  CHECK(parse("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n", r));
  const char* bad[] = {
    "GET / HTTP/1.0\r\nHost: x\r\n\r\n", "GET / HTTP/1.1\r\n\r\n",
    "GET / HTTP/1.1\r\nHost: x\r\nHost: y\r\n\r\n",
    "PUT /api/v1/config HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\n",
    "PUT /api/v1/config HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n",
    "PUT /api/v1/config HTTP/1.1\r\nHost: x\r\nContent-Length: 1025\r\n\r\n",
    "GET / HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\n\r\n",
    "GET / HTTP/1.1\r\n Host: x\r\n\r\n", "GET / HTTP/1.1\r\nHost: x\n\r\n\r\n"
  };
  for (const char* input : bad) CHECK(!parse(input, r));
  const char* token = "0123456789abcdef0123456789abcdef";
  strcpy(r.host, "192.168.4.1"); strcpy(r.origin, "http://192.168.4.1"); strcpy(r.csrf, token);
  CHECK(sameOrigin(r, "192.168.4.1", token));
  strcpy(r.origin, "http://evil.example"); CHECK(!sameOrigin(r, "192.168.4.1", token));
  DigestAuth auth;
  auth.begin("test-only-key", token, 0, hash);
  r = signedRequest(token, 1);
  CHECK(auth.verify(r, 1));
  CHECK(!auth.verify(r, 2)); // replay
  r = signedRequest(token, 2); CHECK(auth.verify(r, 3));
  r = signedRequest(token, 3); strcpy(r.method, "GET"); CHECK(!auth.verify(r, 4));
  r = signedRequest(token, 3); CHECK(!auth.verify(r, kNonceMs));
  for (int i = 0; i < 7; ++i) {
    char cn[20]; snprintf(cn, sizeof(cn), "client-%d", i);
    r = signedRequest(token, 1, cn); CHECK(auth.verify(r, 5));
  }
  r = signedRequest(token, 1, "ninth-client"); CHECK(!auth.verify(r, 6));
  r = signedRequest(token, 3); CHECK(auth.verify(r, 7));
  auth.begin("test-only-key", "abcdef0123456789abcdef0123456789", 9, hash);
  CHECK(!auth.verify(r, 10));
  char digest[65]; hash("abc", digest);
  CHECK(!strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}
void testBootResetAndStorage() {
  BootResetGate gate;
  for (bool power : {false, true}) for (bool pressed : {false, true}) {
    gate.begin(power, pressed, 0);
    CHECK(gate.waiting() == (power && pressed));
    CHECK(gate.tick(true, 3000) == (power && pressed));
    CHECK(!gate.tick(true, 9000));
  }
  gate.begin(true, true, UINT32_MAX - 1000);
  CHECK(!gate.tick(true, 1998)); CHECK(gate.tick(true, 1999));
  gate.begin(true, true, 0);
  CHECK(!gate.tick(false, 1)); CHECK(!gate.tick(true, 4000));
  FakeStorage baseline;
  ConfigStore initial(baseline);
  CHECK(initial.load() == ConfigState::Empty && initial.stage(wifi()) && initial.promote());
  baseline.data["enroll"] = std::vector<uint8_t>(20, 'A');
  baseline.data["unrelated"] = {42};
  auto success = baseline;
  success.operations = 0;
  ConfigStore reset(success);
  CHECK(reset.resetWifi());
  const int resetOperations = success.operations;
  CHECK(reset.load() == ConfigState::Empty && reset.revision() == 0);
  CHECK(success.data.size() == 2 && success.data.at("enroll") == baseline.data.at("enroll"));
  for (bool persisted : {false, true}) for (int failure = 1; failure <= resetOperations; ++failure) {
    auto broken = baseline;
    broken.operations = 0; broken.failAt = failure; broken.persistedFailure = persisted;
    ConfigStore interrupted(broken);
    CHECK(!interrupted.resetWifi());
    CHECK(broken.data.at("enroll") == baseline.data.at("enroll"));
    CHECK(broken.data.at("unrelated") == baseline.data.at("unrelated"));
    broken.failAt = -1;
    ConfigStore restarted(broken);
    CHECK(restarted.resumeReset());
    const bool intentNeverPersisted = failure == 1 && !persisted;
    CHECK(restarted.load() == (intentNeverPersisted ? ConfigState::Active : ConfigState::Empty));
    if (!intentNeverPersisted) CHECK(restarted.revision() == 0 && broken.data.size() == 2);
    CHECK(restarted.resumeReset());
  }
  baseline.data["reset"] = {0};
  ConfigStore corrupt(baseline);
  const auto before = baseline.data;
  CHECK(!corrupt.resumeReset() && baseline.data == before);
  CHECK(corrupt.resetWifi() && corrupt.load() == ConfigState::Empty);
}
void testMdnsOrigin() {
  const char* local = "dm-bridge-8810a1.local";
  for (const char* host : {local, "DM-BRIDGE-8810A1.LOCAL:80", "192.168.1.3", "192.168.1.3:80"})
    CHECK(allowedHost(host, "192.168.1.3", local));
  for (const char* host : {"dm-bridge-other.local", "dm-bridge-8810a1.local.evil", "dm-bridge-8810a1.local:81",
      "dm-bridge-8810a1.local@evil", "dm-bridge-8810a1.local/", "dm-bridge-8810a1.local:80:80", "", "192.168.1.4"})
    CHECK(!allowedHost(host, "192.168.1.3", local));
  CHECK(!allowedHost(local, "192.168.1.3"));
  Request request;
  const char* csrf = "0123456789abcdef0123456789abcdef";
  strcpy(request.csrf, csrf);
  strcpy(request.host, "DM-BRIDGE-8810A1.LOCAL:80");
  strcpy(request.origin, "http://dm-bridge-8810a1.local");
  CHECK(sameOrigin(request, "192.168.1.3", csrf, local));
  for (const char* origin : {"http://192.168.1.3", "https://dm-bridge-8810a1.local", "null",
      "http://dm-bridge-8810a1.local/", "http://dm-bridge-8810a1.local:81", "http://evil.local"}) {
    strcpy(request.origin, origin); CHECK(!sameOrigin(request, "192.168.1.3", csrf, local));
  }
  strcpy(request.origin, "http://dm-bridge-8810a1.local");
  request.csrf[0] = 'f'; CHECK(!sameOrigin(request, "192.168.1.3", csrf, local));
}
void testMalformedCorpus() {
  uint32_t seed = 1;
  for (unsigned trial = 0; trial < 2000; ++trial) {
    char data[1025];
    seed = seed * 1664525U + 1013904223U;
    const size_t size = seed % 1024;
    for (size_t i = 0; i < size; ++i) {
      seed = seed * 1664525U + 1013904223U;
      data[i] = static_cast<char>(seed >> 24);
    }
    data[size] = 0;
    WifiConfig config;
    if (parseConfig(data, size, config)) CHECK(validWifi(config));
    Request request;
    parseHeaders(data, request);
    DigestAuth auth;
    auth.begin("test-only-key", "0123456789abcdef0123456789abcdef", 0, hash);
    memcpy(request.authorization, data, size < 767 ? size : 767);
    request.authorization[767] = 0;
    CHECK(!auth.verify(request, 1));
  }
}
}
void testAutomaticPolicy() {
  FakeStorage storage;
  AutomaticPolicy policy(storage);
  CHECK(policy.load() && !policy.enabled());
  CHECK(policy.save(true));
  const auto saved = storage.data;
  AutomaticPolicy reboot(storage);
  CHECK(reboot.load() && reboot.enabled());
  const auto operations = storage.operations;
  CHECK(reboot.save(true) && storage.operations == operations);
  CHECK(reboot.save(false));
  CHECK(policy.load() && !policy.enabled());
  // Every one-bit record corruption and every truncation fails closed.
  for (unsigned bit = 0; bit < 64; ++bit) {
    storage.data = saved; storage.data["ghauto"][bit / 8] ^= 1U << (bit % 8);
    CHECK(!policy.load() && !policy.enabled() && !policy.healthy());
  }
  for (unsigned length = 0; length < 8; ++length) {
    storage.data = saved; storage.data["ghauto"].resize(length);
    CHECK(!policy.load() && !policy.enabled());
  }
  for (bool persisted : {false, true}) {
    for (int failure : {1, 2}) {
      storage.data = saved; storage.failAt = -1;
      CHECK(policy.load() && policy.enabled());
      storage.failAt = storage.operations + failure;
      storage.persistedFailure = persisted;
      CHECK(!policy.save(false) && !policy.enabled() && !policy.healthy());
      storage.failAt = -1;
      CHECK(reboot.load()); // Old or new exact record may survive: never fake a commit.
    }
  }
  storage.data = saved; storage.failAt = -1;
  ConfigStore config(storage);
  CHECK(config.resetWifi());
  CHECK(!storage.data.count("ghauto") && policy.load() && !policy.enabled());
}
int main() {
  testJsonAndRecords(); testStorage(); testNetwork(); testHeadersAndAuth(); testMalformedCorpus();
  testBootResetAndStorage(); testMdnsOrigin();
  testAutomaticPolicy();
  printf("USB bootstrap host core: %u assertions passed\n", checks);
}
