#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <nvs.h>
#include "vendor/monocypher/monocypher-ed25519.h"

static size_t applicationReads = 0;
static ssize_t countedRecv(int fd, void* buffer, size_t size, int flags) {
  const ssize_t result = ::recv(fd, buffer, size, flags);
  if (result > 0) applicationReads += static_cast<size_t>(result);
  return result;
}
#define recv countedRecv
#include "../../firmware/usb_bootstrap/main.cpp"
#undef recv

uint32_t fakeNow = 0;
FakeSerial Serial;
FakeEsp ESP;
FakeWiFi WiFi;
FakeMdns MDNS;
bool fakeButtonPressed = false;
esp_reset_reason_t fakeResetReason = ESP_RST_POWERON;
static std::map<std::string, std::vector<uint8_t>> fakeNvs;
static unsigned writes = 0;
esp_err_t nvs_open(const char*, int, nvs_handle_t* handle) { *handle = 1; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* out, size_t* size) {
  if (!fakeNvs.count(key)) return ESP_ERR_NVS_NOT_FOUND;
  const auto& bytes = fakeNvs.at(key);
  if (*size < bytes.size()) return 99;
  memcpy(out, bytes.data(), bytes.size()); *size = bytes.size(); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* in, size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(in);
  fakeNvs[key] = std::vector<uint8_t>(bytes, bytes + size); ++writes; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t, const char* key) { fakeNvs.erase(key); return ESP_OK; }
void esp_fill_random(void* out, size_t size) {
  static uint8_t counter = 0;
  auto* bytes = static_cast<uint8_t*>(out);
  for (size_t i = 0; i < size; ++i) bytes[i] = ++counter;
}

namespace {
unsigned checks = 0;
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while (false)
void reset() {
  closeClient();
  fakeNvs.clear(); writes = 0;
  fakeOta() = FakeOta{};
  memset(installKey, 0, sizeof(installKey));
  fakeNow = 0; ESP.restarts = 0; WiFi = FakeWiFi{};
  MDNS = FakeMdns{}; mdnsActive = mdnsAttempted = false; mdnsAttemptAt = 0;
  fakeButtonPressed = false; fakeResetReason = ESP_RST_POWERON;
  network = NetworkState{};
  pendingReboot = wasConnected = false;
  rateWindow = requestsInWindow = authFailures = 0;
  Serial.messages.clear();
  setup();
  CHECK(network.mode() == Mode::Provisioning);
  CHECK(strlen(installKey) == 20 && strlen(csrf) == 32);
  applicationReads = 0;
}
std::string digest(const std::string& method, const std::string& path) {
  char ha1[65], ha2[65], result[65];
  sha256((std::string(kUsername) + ":" + kRealm + ":" + installKey).c_str(), ha1);
  sha256((method + ":" + path).c_str(), ha2);
  sha256((std::string(ha1) + ":" + auth.nonce() + ":00000001:test-client:auth:" + ha2).c_str(), result);
  return std::string("Authorization: Digest username=\"installer\", realm=\"DM-BRIDGE-USB\", nonce=\"") +
    auth.nonce() + "\", uri=\"" + path + "\", response=\"" + result +
    "\", algorithm=SHA-256, qop=auth, nc=00000001, cnonce=\"test-client\"\r\n";
}
int connectRequest(const std::string& payload) {
  int sockets[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
  server.accepted = sockets[0];
  CHECK(::send(sockets[1], payload.data(), payload.size(), 0) == static_cast<ssize_t>(payload.size()));
  return sockets[1];
}
std::string drain(int peer) {
  std::string response;
  for (unsigned i = 0; i < 500; ++i) {
    serviceHttp(++fakeNow);
    char bytes[512];
    const auto count = ::recv(peer, bytes, sizeof(bytes), MSG_DONTWAIT);
    if (count > 0) response.append(bytes, static_cast<size_t>(count));
    if (count == 0) break;
  }
  ::close(peer);
  return response;
}
const std::string json = "{\"ssid\":\"host-test-network\",\"password\":\"host-test-secret\",\"configRevision\":0}";
std::string putHeaders(bool authenticate, bool goodCsrf = true) {
  return "PUT /api/v1/config HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\nContent-Length: " +
    std::to_string(json.size()) + "\r\nOrigin: http://192.168.4.1\r\nX-CSRF-Token: " +
    (goodCsrf ? csrf : "00000000000000000000000000000000") + "\r\n" +
    (authenticate ? digest("PUT", "/api/v1/config") : "") + "\r\n";
}
void firstChunkAuth() {
  reset();
  const unsigned originalWrites = writes;
  const auto headers = putHeaders(false);
  const auto response = drain(connectRequest(headers + json));
  CHECK(response.find("401 Unauthorized") != std::string::npos);
  CHECK(response.find("algorithm=SHA-256") != std::string::npos);
  CHECK(applicationReads == headers.size());
  CHECK(writes == originalWrites && !fakeNvs.count("trial"));
  reset();
  const auto invalid = putHeaders(true, false);
  const auto rejected = drain(connectRequest(invalid + json));
  CHECK(rejected.find("403 Forbidden") != std::string::npos);
  CHECK(applicationReads == invalid.size() && !fakeNvs.count("trial"));
}
void fragmentedSaveAndReboot() {
  reset();
  const auto headers = putHeaders(true);
  const int peer = connectRequest(headers.substr(0, 10));
  serviceHttp(++fakeNow);
  CHECK(!bodyAllowed && !fakeNvs.count("trial"));
  const auto remaining = headers.substr(10) + json.substr(0, 8);
  CHECK(::send(peer, remaining.data(), remaining.size(), 0) == static_cast<ssize_t>(remaining.size()));
  for (unsigned i = 0; i < 10; ++i) serviceHttp(++fakeNow);
  CHECK(bodyAllowed && bodyUsed == 8 && !fakeNvs.count("trial"));
  const auto rest = json.substr(8);
  CHECK(::send(peer, rest.data(), rest.size(), 0) == static_cast<ssize_t>(rest.size()));
  const auto response = drain(peer);
  CHECK(response.find("202 Accepted") != std::string::npos);
  CHECK(response.find("\"rebootScheduled\":true") != std::string::npos);
  CHECK(response.find("host-test-secret") == std::string::npos);
  CHECK(fakeNvs.count("trial") && !fakeNvs.count("active"));
  CHECK(ESP.restarts == 0);
  serviceHttp(++fakeNow); // Response has closed; start the explicit reboot wait.
  CHECK(network.mode() == Mode::Rebooting);
  fakeNow += kRebootMs - 1; loop(); CHECK(ESP.restarts == 0);
  ++fakeNow; loop(); CHECK(ESP.restarts == 1);
  closeClient();
  network = NetworkState{}; pendingReboot = false;
  setup();
  CHECK(network.mode() == Mode::Trial);
  WiFi.linked = true; loop();
  fakeNow += kStableMs; loop();
  CHECK(network.mode() == Mode::Station && fakeNvs.count("active"));
  for (const auto& line : Serial.messages) CHECK(line.find("host-test-secret") == std::string::npos);
  const auto before = fakeNvs;
  WiFi.linked = false; loop(); fakeNow += 1200000u; loop();
  CHECK(network.mode() == Mode::Station && fakeNvs == before);
}
void boundsAndDeadlines() {
  reset();
  const int peer = connectRequest("GET / HTTP/1.1\r\n");
  serviceHttp(++fakeNow); CHECK(client.fd() >= 0);
  fakeNow += kRequestMs; serviceHttp(fakeNow); CHECK(client.fd() < 0); ::close(peer);
  reset();
  const auto huge = std::string("GET / HTTP/1.1\r\nHost: ") + std::string(kHeaderMax, 'a');
  const auto rejected = drain(connectRequest(huge));
  CHECK(rejected.find("431 Request Header Fields Too Large") != std::string::npos);
  CHECK(applicationReads <= kHeaderMax + 1 && !fakeNvs.count("trial"));
  reset();
  const auto headers = putHeaders(true);
  const int stalled = connectRequest(headers + json.substr(0, 1));
  for (unsigned i = 0; i < 10; ++i) serviceHttp(++fakeNow);
  fakeNow += kRequestMs; serviceHttp(fakeNow);
  CHECK(client.fd() < 0 && !fakeNvs.count("trial")); ::close(stalled);
  reset();
  const auto request = "GET /api/v1/status HTTP/1.1\r\nHost: evil.example\r\n" + digest("GET", "/api/v1/status") + "\r\n";
  CHECK(drain(connectRequest(request)).find("HOST_REJECTED") != std::string::npos);
}
void failedTrialReturnsToAp() {
  reset();
  WifiConfig config;
  CHECK(parseConfig(json.data(), json.size(), config));
  CHECK(configStore.stage(config));
  const std::string key = installKey;
  closeClient(); setup();
  CHECK(network.mode() == Mode::Trial);
  fakeNow += kTrialMs; loop();
  CHECK(network.mode() == Mode::Provisioning && !fakeNvs.count("trial"));
  CHECK(std::string(installKey) == key && configStore.revision() == 1);
  fakeNow += 600001u; loop();
  CHECK(network.mode() == Mode::Provisioning && WiFi.currentMode == WIFI_AP);
}
void publicLoginAndUnlimitedAp() {
  reset();
  const unsigned originalWrites = writes;
  fakeNow += 600001u; loop();
  CHECK(network.mode() == Mode::Provisioning && WiFi.currentMode == WIFI_AP);
  auto page = drain(connectRequest("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"));
  CHECK(page.find("200 OK") != std::string::npos);
  CHECK(page.find("text/html") != std::string::npos);
  CHECK(page.find("id=\"login\"") != std::string::npos);
  CHECK(page.find("WWW-Authenticate:") == std::string::npos);
  CHECK(page.find(installKey) == std::string::npos && page.find(csrf) == std::string::npos);
  CHECK(writes == originalWrites);
  auto metadata = drain(connectRequest("GET /api/v1/auth HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"));
  CHECK(metadata.find("200 OK") != std::string::npos);
  CHECK(metadata.find("\"algorithm\":\"SHA-256\"") != std::string::npos);
  CHECK(metadata.find("WWW-Authenticate:") == std::string::npos);
  CHECK(metadata.find(installKey) == std::string::npos && metadata.find(csrf) == std::string::npos);
  const std::string nonce = auth.nonce();
  fakeNow += kNonceMs;
  metadata = drain(connectRequest("GET /api/v1/auth HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"));
  CHECK(metadata.find("200 OK") != std::string::npos && nonce != auth.nonce());
  auto status = drain(connectRequest("GET /api/v1/status HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"));
  CHECK(status.find("401 Unauthorized") != std::string::npos);
  CHECK(status.find(csrf) == std::string::npos);
  status = drain(connectRequest("GET /api/v1/status HTTP/1.1\r\nHost: 192.168.4.1\r\n" + digest("GET", "/api/v1/status") + "\r\n"));
  CHECK(status.find("200 OK") != std::string::npos);
  CHECK(status.find("\"apTimeoutEnabled\":false,\"apRemainingMs\":null") != std::string::npos);
  CHECK(status.find("\"githubTlsAttempted\":false,\"githubTlsConnectResult\":0,\"githubTlsConnectMs\":0") != std::string::npos);
  CHECK(status.find("\"githubTlsEspError\":0,\"githubTlsError\":0,\"githubTlsVerifyFlags\":0") != std::string::npos);
  CHECK(writes == originalWrites);
  const auto badRoot = "GET / HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: 1\r\n\r\nx";
  CHECK(drain(connectRequest(badRoot)).find("200 OK") == std::string::npos);
  CHECK(drain(connectRequest("POST /api/v1/auth HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")).find("401 Unauthorized") != std::string::npos);
  CHECK(drain(connectRequest("GET / HTTP/1.1\r\nHost: evil.example\r\n\r\n")).find("HOST_REJECTED") != std::string::npos);
  authFailures = 5;
  CHECK(drain(connectRequest("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")).find("200 OK") != std::string::npos);
  CHECK(drain(connectRequest("GET /api/v1/auth HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")).find("AUTH_RATE_LIMIT") != std::string::npos);
}
}
void bootButtonAndMdns() {
  reset();
  CHECK(!mdnsActive && MDNS.begins == 0);
  WifiConfig config;
  CHECK(parseConfig(json.data(), json.size(), config) && configStore.stage(config) && configStore.promote());
  const auto original = fakeNvs;
  for (auto reason : {ESP_RST_SW, ESP_RST_BROWNOUT, ESP_RST_TASK_WDT}) {
    fakeButtonPressed = true; fakeResetReason = reason;
    setup(); CHECK(applicationStarted && !bootReset.waiting());
    CHECK(network.mode() == Mode::Station && fakeNvs == original);
  }
  fakeResetReason = ESP_RST_POWERON; fakeButtonPressed = true;
  WiFi = FakeWiFi{};
  setup(); CHECK(!applicationStarted && bootReset.waiting());
  CHECK(WiFi.currentMode == WIFI_OFF && fakeNvs == original);
  fakeNow += 2999; loop(); CHECK(!applicationStarted && fakeNvs == original);
  fakeButtonPressed = false; loop();
  CHECK(applicationStarted && network.mode() == Mode::Station && fakeNvs == original);
  fakeButtonPressed = true; fakeNow += 3001; loop(); CHECK(fakeNvs == original);
  WiFi.linked = true; loop();
  CHECK(mdnsActive && MDNS.begins == 1 && MDNS.services == 1 && MDNS.hostname == deviceName);
  CHECK(std::string(localHostname) == "dm-bridge-112233.local");
  const std::string host = localHostname;
  const auto response = drain(connectRequest("GET /api/v1/status HTTP/1.1\r\nHost: " + host + ":80\r\n" + digest("GET", "/api/v1/status") + "\r\n"));
  CHECK(response.find("200 OK") != std::string::npos && response.find("\"mdnsActive\":true") != std::string::npos);
  CHECK(response.find(installKey) == std::string::npos && response.find(kBuildId) != std::string::npos);
  CHECK(drain(connectRequest("GET / HTTP/1.1\r\nHost: " + host + ".evil\r\n\r\n")).find("HOST_REJECTED") != std::string::npos);
  CHECK(drain(connectRequest("GET /api/v1/status HTTP/1.1\r\nHost: " + host + "\r\n\r\n")).find("401 Unauthorized") != std::string::npos);
  loop(); CHECK(MDNS.begins == 1);
  WiFi.linked = false; loop(); CHECK(!mdnsActive && MDNS.ends == 1);
  MDNS.beginOk = false; WiFi.linked = true; loop();
  CHECK(!mdnsActive && MDNS.begins == 2 && MDNS.ends == 2 && network.mode() == Mode::Station);
  CHECK(drain(connectRequest("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")).find("200 OK") != std::string::npos);
  const auto attempt = mdnsAttemptAt;
  fakeNow = attempt + kMdnsRetryMs - 1; loop(); CHECK(MDNS.begins == 2);
  MDNS.beginOk = true; MDNS.serviceOk = false;
  ++fakeNow; loop(); CHECK(!mdnsActive && MDNS.ends == 3 && MDNS.services == 2);
  MDNS.serviceOk = true; fakeNow += kMdnsRetryMs; loop(); CHECK(mdnsActive);
  fault("TEST_FAULT"); CHECK(!mdnsActive && WiFi.currentMode == WIFI_OFF);
  fakeButtonPressed = true; setup();
  const auto started = fakeNow;
  fakeNow = started + 2999; loop(); CHECK(fakeNvs == original);
  ++fakeNow; loop();
  CHECK(network.mode() == Mode::Provisioning && fakeNvs.size() == 1);
  CHECK(fakeNvs.at("enroll") == original.at("enroll") && configStore.revision() == 0);
  CHECK(!mdnsActive);
  const auto afterReset = fakeNvs;
  fakeNow += 4000; loop(); CHECK(fakeNvs == afterReset);
  CHECK(drain(connectRequest("GET / HTTP/1.1\r\nHost: " + host + "\r\n\r\n")).find("HOST_REJECTED") != std::string::npos);
}
void otaHttpBoundary() {
  const auto station = []() {
    reset();
    WifiConfig config; strcpy(config.ssid, "host-test"); strcpy(config.password, "password-only-fixture");
    CHECK(configStore.stage(config) && configStore.promote());
    applyAction(network.begin(ConfigState::Active, fakeNow), fakeNow);
    WiFi.linked = true; CHECK(otaHealthy()); applicationReads = 0;
  };
  const auto headers = [](const char* path, size_t size, bool authenticated, bool goodCsrf, const char* token = "") {
    rotateNonce(++fakeNow);
    return std::string("POST ") + path + " HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
      std::to_string(size) + "\r\nOrigin: http://192.168.4.1\r\nX-CSRF-Token: " + (goodCsrf ? csrf : "wrong") +
      "\r\nX-OTA-Token: " + token + "\r\n" + (authenticated ? digest("POST", path) : "") + "\r\n";
  };
  for (unsigned kind = 0; kind < 3; ++kind) {
    station(); const unsigned before = writes;
    const auto head = headers("/api/v1/ota/upload", 2300, kind != 0, kind != 1, "bad-token");
    const auto reply = drain(connectRequest(head + std::string(2300, 'x')));
    CHECK(reply.find(kind == 0 ? "401 Unauthorized" : kind == 1 ? "403 Forbidden" : "409 Conflict") != std::string::npos);
    CHECK(applicationReads == head.size() && writes == before && !fakeOta().begins && !fakeOta().selects);
  }
  station();
  uint8_t seed[32] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
    0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
  uint8_t secret[64], pub[32], envelope[ota::kEnvelopeBytes];
  crypto_ed25519_key_pair(secret, pub, seed); CHECK(!memcmp(pub, kOtaPublicKey, 32));
  const std::string image(2300, 'x');
  ota::Manifest manifest; manifest.version = kOtaVersion + 1; manifest.imageSize = image.size(); manifest.minSchema = 1;
  strcpy(manifest.boardId, ota::kBoardId); strcpy(manifest.buildId, "host-test-next");
  mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(image.data()), image.size(), manifest.sha256, 0);
  ota::encodeManifest(manifest, envelope);
  crypto_ed25519_sign(envelope + ota::kManifestBytes, secret, envelope, ota::kManifestBytes);
  envelope[179] ^= 1;
  auto reply = drain(connectRequest(headers("/api/v1/ota/prepare", sizeof(envelope), true, true) +
    std::string(reinterpret_cast<const char*>(envelope), sizeof(envelope))));
  CHECK(reply.find("SIGNATURE_INVALID") != std::string::npos && !fakeOta().begins && !fakeNvs.count("ota"));
  envelope[179] ^= 1;
  reply = drain(connectRequest(headers("/api/v1/ota/prepare", sizeof(envelope), true, true) +
    std::string(reinterpret_cast<const char*>(envelope), sizeof(envelope))));
  CHECK(reply.find("200 OK") != std::string::npos && !fakeOta().begins);
  const std::string token = otaRuntime.updater.transaction();
  const auto preservedActive = fakeNvs["active"], preservedEnroll = fakeNvs["enroll"];
  reply = drain(connectRequest(headers("/api/v1/ota/upload", image.size(), true, true, token.c_str()) + image));
  CHECK(reply.find("202 Accepted") != std::string::npos && fakeOta().selects == 1 && fakeOta().ends == 1);
  CHECK(fakeNvs["active"] == preservedActive && fakeNvs["enroll"] == preservedEnroll && fakeNvs.count("ota"));
  CHECK(ESP.restarts == 0 && pendingReboot);
  CHECK(verifyRollbackLater());
  uint8_t package[ota::kPackageHeaderBytes];
  manifest.minUpdater = 2; strcpy(manifest.channel, "stable");
  ota::encodePackageManifest(manifest, package);
  crypto_ed25519_sign(package + ota::kPackageManifestBytes, secret, package, ota::kPackageManifestBytes);
  for (unsigned kind = 0; kind < 5; ++kind) {
    station();
    if (kind == 2) applyAction(network.begin(ConfigState::Empty, fakeNow), fakeNow);
    const auto head = headers("/api/v1/ota/file/prepare", sizeof(package), kind != 0, kind != 1);
    if (kind == 3) package[191] ^= 1;
    reply = drain(connectRequest(head + std::string(reinterpret_cast<const char*>(package), sizeof(package))));
    if (kind == 3) package[191] ^= 1;
    CHECK(!fakeOta().begins && !fakeNvs.count("otactx2"));
    if (kind < 3) CHECK(applicationReads == head.size());
    if (kind < 4) { CHECK(reply.find("uploadToken") == std::string::npos); continue; }
    CHECK(reply.find("200 OK") != std::string::npos);
    CHECK(otaRuntime.updater.origin() == ota::Origin::WebFile);
    const std::string fileToken = otaRuntime.updater.transaction();
    const auto active = fakeNvs["active"], enroll = fakeNvs["enroll"];
    reply = drain(connectRequest(headers("/api/v1/ota/upload", image.size(), true, true, fileToken.c_str()) + image));
    CHECK(reply.find("\"confirmationRequired\":false") != std::string::npos);
    CHECK(reply.find("\"localHealthRequired\":true") != std::string::npos);
    CHECK(fakeOta().selects == 1 && fakeNvs["ota"].size() == ota::kJournalBytes);
    CHECK(fakeNvs.count("otactx2") && fakeNvs["active"] == active && fakeNvs["enroll"] == enroll);
  }
  for (unsigned kind = 0; kind < 5; ++kind) {
    station();
    CHECK(!githubPull.automatic() && !githubPull.busy());
    if (kind == 2) applyAction(network.begin(ConfigState::Empty, fakeNow), fakeNow);
    const unsigned before = writes;
    const auto head = headers("/api/v1/ota/github/check", kind == 3 ? 1 : 0, kind != 0, kind != 1);
    reply = drain(connectRequest(head + (kind == 3 ? "x" : "")));
    CHECK(applicationReads == head.size() && writes == before && !fakeOta().begins);
    CHECK(githubPull.busy() == (kind == 4));
    if (kind < 4) CHECK(reply.find("202 Accepted") == std::string::npos);
    else {
      CHECK(reply.find("\"installIfNewer\":true") != std::string::npos);
      reply = drain(connectRequest(headers("/api/v1/ota/github/check", 0, true, true)));
      CHECK(reply.find("CHECK_BUSY_OR_RATE_LIMITED") != std::string::npos);
      reply = drain(connectRequest(headers("/api/v1/ota/file/prepare", sizeof(package), true, true)));
      CHECK(reply.find("OTA_BUSY") != std::string::npos);
    }
  }
}
int main(int argc, char** argv) {
  if (argc == 2 && !strcmp(argv[1], "--curl-server")) {
    reset();
    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listener >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    CHECK(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    CHECK(listen(listener, 1) == 0);
    CHECK(fcntl(listener, F_SETFL, O_NONBLOCK) == 0);
    // Synthetic test fixture only; no real USB device or credentials exist here.
    printf("%u %s\n", ntohs(address.sin_port), installKey); fflush(stdout);
    for (unsigned step = 0; step < 15000; ++step) {
      if (client.fd() < 0 && server.accepted < 0) server.accepted = accept(listener, nullptr, nullptr);
      serviceHttp(++fakeNow);
      usleep(1000);
    }
    closeClient(); ::close(listener);
    return 0;
  }
  firstChunkAuth(); fragmentedSaveAndReboot(); boundsAndDeadlines(); failedTrialReturnsToAp(); publicLoginAndUnlimitedAp();
  bootButtonAndMdns();
  otaHttpBoundary();
  printf("USB bootstrap transport/runtime fakes: %u assertions passed\n", checks);
}
