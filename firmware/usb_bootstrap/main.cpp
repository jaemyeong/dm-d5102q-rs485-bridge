#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <nvs.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <lwip/sockets.h>
#include <errno.h>

#include "core.h"
#include "boot_button.h"
#include "web_ui.h"
#include "ota_runtime.h"

#if defined(ARDUINO) && !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
#error "B1 requires a rollback-enabled pinned ESP32 SDK/bootloader"
#endif
// Override the Arduino C weak hook: application health, not initArduino, commits.
extern "C" __attribute__((used, noinline)) bool verifyRollbackLater() { return true; }

namespace {
using namespace bootstrap;
class NvsStorage final : public Storage {
 public:
  nvs_handle_t handle = 0;
  size_t read(const char* key, uint8_t* data, size_t capacity) override {
    size_t size = capacity;
    const esp_err_t result = nvs_get_blob(handle, key, data, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) return 0;
    return result == ESP_OK && size > 0 && size <= capacity ? size : SIZE_MAX;
  }
  bool write(const char* key, const uint8_t* data, size_t size) override {
    return nvs_set_blob(handle, key, data, size) == ESP_OK && nvs_commit(handle) == ESP_OK;
  }
  bool erase(const char* key) override {
    const esp_err_t result = nvs_erase_key(handle, key);
    return result == ESP_ERR_NVS_NOT_FOUND || (result == ESP_OK && nvs_commit(handle) == ESP_OK);
  }
} storage;
ota::Runtime otaRuntime(storage);
ConfigStore configStore(storage);
NetworkState network;
BootResetGate bootReset;
bool applicationStarted = false;
bool mdnsActive = false, mdnsAttempted = false;
uint32_t mdnsAttemptAt = 0;
char localHostname[32] = {};
DigestAuth auth;
WiFiServer server(80, 1);
WiFiClient client;
char installKey[21] = {};
char csrf[33] = {};
char deviceName[24] = {};
char apName[24] = {};
char otaKeyId[65] = {};
char requestBytes[kHeaderMax + 1] = {};
char body[kBodyMax + 1] = {};
Request request;
size_t requestUsed = 0, bodyUsed = 0;
bool bodyAllowed = false, uploadAllowed = false;
char responseHeaders[768] = {};
char responseJson[1536] = {};
const char* responseBody = nullptr;
size_t headersSent = 0, responseSent = 0, responseSize = 0;
uint32_t connectedAt = 0, responseAt = 0, rateWindow = 0;
unsigned requestsInWindow = 0, authFailures = 0;
bool pendingReboot = false, wasConnected = false;

void randomHex(char out[33]) {
  uint8_t bytes[16];
  esp_fill_random(bytes, sizeof(bytes));
  constexpr char alphabet[] = "0123456789abcdef";
  for (unsigned i = 0; i < 16; ++i) { out[2*i] = alphabet[bytes[i] >> 4]; out[2*i+1] = alphabet[bytes[i] & 15]; }
  out[32] = 0;
}
void sha256(const char* input, char out[65]) {
  uint8_t digest[32];
  if (mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(input), strlen(input), digest, 0) != 0) {
    out[0] = 0;
    return;
  }
  constexpr char alphabet[] = "0123456789abcdef";
  for (unsigned i = 0; i < 32; ++i) { out[2*i] = alphabet[digest[i] >> 4]; out[2*i+1] = alphabet[digest[i] & 15]; }
  out[64] = 0;
}
void rotateNonce(uint32_t now) {
  char nonce[33];
  randomHex(nonce);
  auth.begin(installKey, nonce, now, sha256);
}
void closeClient() {
  if (uploadAllowed) otaRuntime.updater.interrupt();
  client.stop();
  memset(requestBytes, 0, sizeof(requestBytes));
  memset(body, 0, sizeof(body));
  request = Request{};
  requestUsed = bodyUsed = headersSent = responseSent = responseSize = 0;
  responseBody = nullptr;
  bodyAllowed = uploadAllowed = false;
}
void fault(const char* code) {
  if (mdnsActive) MDNS.end();
  mdnsActive = false;
  network.fault();
  closeClient();
  server.end();
  WiFi.mode(WIFI_OFF);
  Serial.println(code); // Fixed metadata only, never data/credentials.
}
bool enrollment(bool unconfigured) {
  uint8_t stored[21];
  size_t size = storage.read("enroll", stored, sizeof(stored));
  if (size) {
    if (size != 20) return false;
    for (unsigned i = 0; i < 20; ++i) {
      if (!strchr("ABCDEFGHJKLMNPQRSTUVWXYZ23456789", stored[i]) || !stored[i]) return false;
      installKey[i] = static_cast<char>(stored[i]);
    }
    Serial.println("INSTALL_KEY_REUSED");
    return true;
  }
  if (!unconfigured) return false;
  // Wi-Fi is enabled before this call so ESP32's RNG has RF entropy.
  constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  uint8_t random[20];
  esp_fill_random(random, sizeof(random));
  static_assert(sizeof(alphabet) == 33, "32-symbol enrollment alphabet");
  for (unsigned i = 0; i < 20; ++i) {
    installKey[i] = alphabet[random[i] & 31];
  }
  if (!storage.write("enroll", reinterpret_cast<const uint8_t*>(installKey), 20) ||
      storage.read("enroll", stored, sizeof(stored)) != 20 || memcmp(stored, installKey, 20)) return false;
  // Sole documented first-boot secret handoff. Do not capture to shared logs.
  Serial.print("INSTALL_KEY_ONCE=");
  Serial.println(installKey);
  Serial.println("RETAIN_KEY_PRIVATELY username=installer");
  return true;
}
const char* modeName() {
  switch (network.mode()) {
    case Mode::Provisioning: return "AP";
    case Mode::Trial: return "STA_TRIAL";
    case Mode::Station: return "STA";
    case Mode::Rebooting: return "REBOOTING";
    case Mode::Fault: return "FAULT";
    default: return "STOPPED";
  }
}
void applyAction(Action action, uint32_t now) {
  switch (action) {
    case Action::StartAp:
      if (mdnsActive) MDNS.end();
      mdnsActive = mdnsAttempted = false;
      closeClient();
      server.end();
      WiFi.disconnect(false, false);
      if (!WiFi.mode(WIFI_AP) || !WiFi.softAP(apName, installKey, 1, false, 1)) { fault("AP_START_FAILED"); break; }
      server.begin();
      if (!server) { fault("HTTP_START_FAILED"); break; }
      Serial.print("AP_SSID="); Serial.println(apName);
      Serial.print("AP_URL=http://"); Serial.println(WiFi.softAPIP());
      break;
    case Action::Connect:
      if (WiFi.getMode() != WIFI_STA) {
        server.end();
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(deviceName);
        server.begin();
        if (!server) { fault("HTTP_START_FAILED"); break; }
      }
      WiFi.begin(configStore.config().ssid, configStore.config().password);
      Serial.println("STA_CONNECT_ATTEMPT");
      break;
    case Action::Promote:
      if (!configStore.promote()) fault("CONFIG_COMMIT_FAILED");
      else { network.promoted(); Serial.println("STA_CONFIG_ACTIVE"); }
      break;
    case Action::DiscardTrial:
      if (!configStore.discardTrial()) fault("CONFIG_WITHDRAW_FAILED");
      else {
        Serial.println("STA_TRIAL_FAILED RETURNING_TO_AP");
        applyAction(network.begin(ConfigState::Empty, now), now);
      }
      break;
    case Action::Reboot: ESP.restart(); break;
    default: break;
  }
}
void respond(unsigned status, const char* reason, const char* payload, const char* type = "application/json", const char* extra = "") {
  responseBody = payload;
  responseSize = strlen(payload);
  headersSent = responseSent = 0;
  responseAt = millis();
  const int count = snprintf(responseHeaders, sizeof(responseHeaders),
    "HTTP/1.1 %u %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %u\r\nConnection: close\r\n"
    "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"
    "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'\r\n%s\r\n",
    status, reason, type, static_cast<unsigned>(responseSize), extra);
  if (count < 0 || static_cast<size_t>(count) >= sizeof(responseHeaders)) closeClient();
}
void error(unsigned code, const char* reason, const char* label) {
  snprintf(responseJson, sizeof(responseJson), "{\"error\":\"%s\"}", label);
  respond(code, reason, responseJson);
}
void challenge(uint32_t now) {
  rotateNonce(now);
  char header[224];
  snprintf(header, sizeof(header), "WWW-Authenticate: Digest realm=\"%s\", nonce=\"%s\", algorithm=SHA-256, qop=\"auth\"\r\n", kRealm, auth.nonce());
  respond(401, "Unauthorized", "{\"error\":\"AUTH_REQUIRED\"}", "application/json", header);
}
bool otaHealthy() {
  return applicationStarted && configStore.state() == ConfigState::Active &&
    network.mode() == Mode::Station && WiFi.status() == WL_CONNECTED &&
    uint32_t(WiFi.localIP()) != 0 && bool(server) &&
    heap_caps_get_free_size(MALLOC_CAP_8BIT) >= 81920 &&
    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= 32768;
}
void headerGate(uint32_t now) {
  if (!parseHeaders(requestBytes, request)) { error(400, "Bad Request", "BAD_HEADERS"); return; }
  const IPAddress address = client.localIP();
  char ip[16];
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
  const char* hostname = WiFi.getMode() == WIFI_STA ? localHostname : "";
  if (!allowedHost(request.host, ip, hostname)) { error(403, "Forbidden", "HOST_REJECTED"); return; }
  const bool bodylessGet = !strcmp(request.method, "GET") && request.contentLength == 0;
  // Public static shell only: no state, secrets or CSRF token are embedded.
  if (bodylessGet && !strcmp(request.path, "/")) { respond(200, "OK", kPage, "text/html"); return; }
  if (authFailures >= 5) { error(429, "Too Many Requests", "AUTH_RATE_LIMIT"); return; }
  if (bodylessGet && !strcmp(request.path, "/api/v1/auth")) {
    if (auth.expired(now)) rotateNonce(now);
    snprintf(responseJson, sizeof(responseJson),
      "{\"realm\":\"%s\",\"nonce\":\"%s\",\"algorithm\":\"SHA-256\",\"qop\":\"auth\"}", kRealm, auth.nonce());
    respond(200, "OK", responseJson);
    return;
  }
  if (!auth.verify(request, now)) {
    if (request.authorization[0]) ++authFailures;
    challenge(now);
    return;
  }
  if (!strcmp(request.method, "GET") && request.contentLength == 0) {
    if (!strcmp(request.path, "/api/v1/status") || !strcmp(request.path, "/api/v1/ota/status")) {
      const bool configurable = network.mode() == Mode::Provisioning && configStore.state() == ConfigState::Empty;
      snprintf(responseJson, sizeof(responseJson),
        "{\"buildId\":\"%s\",\"mode\":\"%s\",\"ip\":\"%s\",\"connected\":%s,\"configRevision\":%lu,"
        "\"canConfigure\":%s,\"apTimeoutEnabled\":false,\"apRemainingMs\":null,\"csrfToken\":\"%s\",\"txBlocked\":true,\"otaSupported\":%s,"
        "\"hostname\":\"%s\",\"mdnsUrl\":\"http://%s\",\"mdnsActive\":%s,"
        "\"deviceId\":\"%s\",\"boardId\":\"%s\",\"otaVersion\":%lu,\"configSchema\":%lu,"
        "\"otaPhase\":\"%s\",\"otaReason\":\"%s\",\"otaBootState\":\"%s\","
        "\"otaTransaction\":\"%s\",\"otaExpectedBuild\":\"%s\",\"otaHealthy\":%s,\"otaReceived\":%lu,"
        "\"freeHeap\":%u,\"largestFreeBlock\":%u,\"otaKeyId\":\"%s\",\"releaseTag\":\"%s\"}",
        kBuildId, modeName(), ip, WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) != 0 ? "true" : "false",
        static_cast<unsigned long>(configStore.revision()), configurable ? "true" : "false",
        csrf, otaRuntime.updater.enabled() ? "true" : "false", localHostname, localHostname, mdnsActive ? "true" : "false",
        deviceName, ota::kBoardId, static_cast<unsigned long>(kOtaVersion), static_cast<unsigned long>(kConfigSchema),
        ota::phaseName(otaRuntime.updater.phase()), otaRuntime.updater.reason(), otaRuntime.bootState(),
        otaRuntime.transaction(), otaRuntime.expectedBuild(), otaHealthy() ? "true" : "false",
        static_cast<unsigned long>(otaRuntime.updater.received()),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)), otaKeyId, kReleaseTag);
      respond(200, "OK", responseJson);
      return;
    }
    error(404, "Not Found", "NOT_FOUND"); return;
  }
  if (!strncmp(request.path, "/api/v1/ota/", 12)) {
    if (strcmp(request.method, "POST")) { error(405, "Method Not Allowed", "UNSUPPORTED_ROUTE"); return; }
    if (!sameOrigin(request, ip, csrf, hostname)) { error(403, "Forbidden", "CSRF_REJECTED"); return; }
    if (!strcmp(request.path, "/api/v1/ota/confirm")) {
      if (!request.hasLength || request.contentLength ||
          !otaRuntime.confirm(request.uploadToken, otaHealthy(), now)) {
        error(409, "Conflict", "CONFIRM_REJECTED"); return;
      }
      respond(200, "OK", "{\"confirmed\":true}"); return;
    }
    if (!otaRuntime.canUpdate() || !otaHealthy() || pendingReboot) { error(409, "Conflict", "OTA_NOT_READY"); return; }
    if (!request.hasLength || !request.contentLength || strcmp(request.contentType, "application/octet-stream")) {
      error(400, "Bad Request", "BINARY_LENGTH_REQUIRED"); return;
    }
    if (!strcmp(request.path, "/api/v1/ota/prepare") && request.contentLength == ota::kEnvelopeBytes) {
      bodyAllowed = true; return;
    }
    if (!strcmp(request.path, "/api/v1/ota/upload")) {
      if (!otaRuntime.updater.start(request.uploadToken, request.contentLength, now)) {
        error(409, "Conflict", otaRuntime.updater.reason()); return;
      }
      uploadAllowed = true; return;
    }
    error(400, "Bad Request", "OTA_ROUTE_OR_LENGTH"); return;
  }
  if (strcmp(request.method, "PUT") || strcmp(request.path, "/api/v1/config")) { error(405, "Method Not Allowed", "UNSUPPORTED_ROUTE"); return; }
  if (network.mode() != Mode::Provisioning || configStore.state() != ConfigState::Empty) { error(409, "Conflict", "INITIAL_AP_ONLY"); return; }
  if (!sameOrigin(request, ip, csrf, hostname)) { error(403, "Forbidden", "CSRF_REJECTED"); return; }
  if (!request.hasLength || !request.contentLength || strcmp(request.contentType, "application/json")) { error(400, "Bad Request", "JSON_LENGTH_REQUIRED"); return; }
  bodyAllowed = true; // Only now may application body reads/storage occur.
}
void saveConfig() {
  WifiConfig candidate;
  if (!parseConfig(body, bodyUsed, candidate)) { error(422, "Unprocessable Content", "INVALID_WIFI_CONFIG"); return; }
  if (candidate.revision != configStore.revision()) { error(409, "Conflict", "REVISION_CONFLICT"); return; }
  if (configStore.revision() == UINT32_MAX) { error(409, "Conflict", "REVISION_EXHAUSTED"); return; }
  if (!configStore.stage(candidate)) { fault("CONFIG_SAVE_FAILED STATE_UNKNOWN_USB_RECOVERY"); return; }
  memset(body, 0, sizeof(body));
  snprintf(responseJson, sizeof(responseJson),
    "{\"appliedNow\":[],\"restartRequired\":[\"wifi\"],\"reconnectExpected\":true,\"configRevision\":%lu,"
    "\"effectiveConfig\":{\"wifi\":{\"configured\":false,\"trialPending\":true}},\"rebootScheduled\":true}",
    static_cast<unsigned long>(configStore.revision()));
  pendingReboot = true;
  respond(202, "Accepted", responseJson);
}
void processBody(uint32_t now) {
  if (!strcmp(request.path, "/api/v1/config")) { saveConfig(); return; }
  char token[33]; randomHex(token);
  if (!otaRuntime.updater.prepare(reinterpret_cast<const uint8_t*>(body), bodyUsed, token, now)) {
    error(422, "Unprocessable Content", otaRuntime.updater.reason()); return;
  }
  memset(body, 0, sizeof(body));
  snprintf(responseJson, sizeof(responseJson), "{\"uploadToken\":\"%s\",\"expiresInMs\":%lu}",
    otaRuntime.updater.transaction(), static_cast<unsigned long>(ota::kPrepareMs));
  respond(200, "OK", responseJson);
}
void serviceHttp(uint32_t now) {
  if (elapsed(now, rateWindow, 10000)) { rateWindow = now; requestsInWindow = authFailures = 0; }
  if (pendingReboot && !responseBody) { network.saved(now); pendingReboot = false; return; }
  if (network.mode() == Mode::Fault || network.mode() == Mode::Stopped || network.mode() == Mode::Rebooting) return;
  if (client.fd() < 0) {
    client = server.available();
    if (client.fd() < 0) return;
    connectedAt = now;
    if (++requestsInWindow > 32) { error(429, "Too Many Requests", "REQUEST_RATE_LIMIT"); return; }
  }
  if (responseBody) {
    if (elapsed(now, responseAt, kRequestMs)) { closeClient(); return; }
    const size_t headerLength = strlen(responseHeaders);
    const bool header = headersSent < headerLength;
    const size_t offset = header ? headersSent : responseSent;
    const size_t remaining = (header ? headerLength : responseSize) - offset;
    if (!remaining) { closeClient(); return; }
    const size_t chunk = remaining < kIoPerLoop ? remaining : kIoPerLoop;
    const int sent = ::send(client.fd(), (header ? responseHeaders : responseBody) + offset, chunk, MSG_DONTWAIT);
    if (sent > 0) {
      if (header) headersSent += static_cast<size_t>(sent); else responseSent += static_cast<size_t>(sent);
    } else if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) closeClient();
    return;
  }
  if (uploadAllowed) {
    if (otaRuntime.updater.phase() != ota::Phase::Receiving) { error(408, "Request Timeout", otaRuntime.updater.reason()); uploadAllowed = false; return; }
    uint8_t chunk[ota::kChunkMax];
    const size_t left = request.contentLength - otaRuntime.updater.received();
    const size_t count = left < sizeof(chunk) ? left : sizeof(chunk);
    const int received = ::recv(client.fd(), chunk, count, MSG_DONTWAIT);
    if (received <= 0) {
      if (!received || (errno != EAGAIN && errno != EWOULDBLOCK)) closeClient();
      return;
    }
    if (!otaRuntime.updater.chunk(chunk, static_cast<size_t>(received), now)) {
      uploadAllowed = false; error(422, "Unprocessable Content", otaRuntime.updater.reason()); return;
    }
    if (otaRuntime.updater.phase() == ota::Phase::RebootPending) {
      uploadAllowed = false; pendingReboot = true;
      respond(202, "Accepted", "{\"rebootScheduled\":true,\"confirmationRequired\":true}");
    }
    return;
  }
  if (elapsed(now, connectedAt, kRequestMs)) { closeClient(); return; }
  for (size_t budget = 0; budget < kIoPerLoop; ++budget) {
    char byte;
    const int received = ::recv(client.fd(), &byte, 1, MSG_DONTWAIT);
    if (received <= 0) {
      if (!received || (errno != EAGAIN && errno != EWOULDBLOCK)) closeClient();
      return;
    }
    if (bodyAllowed) {
      body[bodyUsed++] = byte;
      if (bodyUsed == request.contentLength) { body[bodyUsed] = 0; processBody(now); return; }
    } else {
      if (!byte || requestUsed >= kHeaderMax) { error(431, "Request Header Fields Too Large", "HEADER_LIMIT"); return; }
      requestBytes[requestUsed++] = byte;
      requestBytes[requestUsed] = 0;
      if (requestUsed >= 4 && !memcmp(requestBytes + requestUsed - 4, "\r\n\r\n", 4)) { headerGate(now); return; }
    }
  }
}
}

void startApplication(bool resetRequested) {
  applicationStarted = true;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  if (nvs_open("dmboot", NVS_READWRITE, &storage.handle) != ESP_OK) { fault("NVS_OPEN_FAILED"); return; }
  uint8_t marker[8];
  const bool resetPending = storage.read("reset", marker, sizeof(marker)) != 0;
  if ((resetRequested || resetPending) && !enrollment(false)) { fault("ENROLLMENT_CORRUPT USB_RECOVERY_REQUIRED"); return; }
  if (!(resetRequested ? configStore.resetWifi() : configStore.resumeReset())) { fault("WIFI_RESET_FAILED USB_RECOVERY_REQUIRED"); return; }
  if (resetRequested || resetPending) Serial.println("WIFI_RESET_COMPLETE INSTALL_KEY_PRESERVED");
  const ConfigState state = configStore.load();
  if (state == ConfigState::Corrupt) { fault("CONFIG_CORRUPT USB_RECOVERY_REQUIRED"); return; }
  if (!WiFi.mode(WIFI_STA)) { fault("WIFI_INIT_FAILED"); return; }
  if (!enrollment(state == ConfigState::Empty)) { fault("ENROLLMENT_CORRUPT USB_RECOVERY_REQUIRED"); return; }
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceName, sizeof(deviceName), "dm-bridge-%06lx", static_cast<unsigned long>((mac >> 24) & 0xffffff));
  snprintf(localHostname, sizeof(localHostname), "%s.local", deviceName);
  snprintf(apName, sizeof(apName), "DM-BRIDGE-%06lX", static_cast<unsigned long>((mac >> 24) & 0xffffff));
  WiFi.setHostname(deviceName);
  randomHex(csrf);
  uint8_t keyHash[32];
  if (mbedtls_sha256_ret(kOtaPublicKey, sizeof(kOtaPublicKey), keyHash, 0)) { fault("OTA_KEY_HASH_FAILED"); return; }
  for (unsigned i = 0; i < 32; ++i) snprintf(otaKeyId + 2*i, 3, "%02x", keyHash[i]);
  rotateNonce(millis());
  // The STA server must also start when setup already selected WIFI_STA.
  if (state != ConfigState::Empty) { server.begin(); if (!server) { fault("HTTP_START_FAILED"); return; } }
  otaRuntime.begin(millis());
  Serial.print("OTA_BOOT_STATE="); Serial.println(otaRuntime.bootState());
  Serial.println(otaRuntime.updater.enabled() ? "OTA_KEY_CONFIGURED" : "OTA_KEY_MISSING");
  applyAction(network.begin(state, millis()), millis());
}
void setup() {
  Serial.begin(115200);
  otaRuntime.arm(millis()); // Covers initialization faults before NVS/network startup.
  Serial.print("BOOT_BUILD="); Serial.println(kBuildId);
  Serial.println("RS485_DISABLED SIGNED_OTA_BASELINE");
  prepareBootButton();
  bootReset.begin(esp_reset_reason() == ESP_RST_POWERON, bootButtonPressed(), millis());
  applicationStarted = false;
  if (bootReset.waiting()) Serial.println("BOOT_RESET_ARMED HOLD_3S");
  else { Serial.println("BOOT_RESET_NOT_ARMED"); startApplication(false); }
}
void serviceMdns(uint32_t now, bool connected) {
  const bool eligible = connected && WiFi.getMode() == WIFI_STA &&
    (network.mode() == Mode::Trial || network.mode() == Mode::Station);
  if (!eligible) {
    if (mdnsActive) MDNS.end();
    mdnsActive = mdnsAttempted = false;
    return;
  }
  if (mdnsActive || (mdnsAttempted && !elapsed(now, mdnsAttemptAt, kMdnsRetryMs))) return;
  mdnsAttempted = true;
  mdnsAttemptAt = now;
  if (!MDNS.begin(deviceName) || !MDNS.addService("http", "tcp", 80)) {
    MDNS.end(); // begin can fail after initializing the responder.
    Serial.println("MDNS_START_FAILED IP_ACCESS_AVAILABLE");
    return;
  }
  mdnsActive = true;
  Serial.print("MDNS_URL=http://"); Serial.println(localHostname);
}
void loop() {
  const uint32_t now = millis();
  if (!applicationStarted) {
    const bool resetRequested = bootReset.tick(bootButtonPressed(), now);
    if (bootReset.waiting()) return;
    if (!resetRequested) Serial.println("BOOT_RESET_CANCELLED");
    startApplication(resetRequested);
    return;
  }
  otaRuntime.poll(now);
  const bool connected = WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) != 0;
  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) { Serial.print("STA_IP="); Serial.println(WiFi.localIP()); }
    else Serial.println("STA_DISCONNECTED");
  }
  // A committed save finishes its bounded response before entering reboot wait.
  if (!pendingReboot) applyAction(network.tick(now, connected, esp_random()), now);
  serviceMdns(now, connected);
  serviceHttp(now);
  otaRuntime.heartbeat();
}
