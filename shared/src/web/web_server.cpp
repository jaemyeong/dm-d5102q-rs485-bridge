#include "web_server.h"

#include <WiFi.h>
#include "admin_ui.h"
#include "../utils/hex.h"
#include "../build_info.h"

namespace dm {

namespace {

String jsonString(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  return out;
}

void sendAdminUi(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", kAdminIndexHtmlGz, kAdminIndexHtmlGzLen);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

}  // namespace

void WebServer::begin(DeviceConfig& config, ConfigStore& store, DeviceStatus& status, BaudScanner& scanner) {
  config_ = &config;
  store_ = &store;
  status_ = &status;
  scanner_ = &scanner;

  authMiddleware_.setAuthType(AsyncAuthType::AUTH_BASIC);
  authMiddleware_.setRealm("DM-D5102Q Bridge");
  authMiddleware_.setAuthFailureMessage("Authentication required");
  authMiddleware_.setUsername(config_->security.username.c_str());
  authMiddleware_.setPassword(config_->security.password.c_str());
  authMiddleware_.generateHash();
  server_.addMiddleware(&authMiddleware_);
  ws_.addMiddleware(&authMiddleware_);

  ws_.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    handleWsEvent(server, client, type, arg, data, len);
  });
  server_.addHandler(&ws_);
  registerRoutes();
  ota_.begin(server_, status);
  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET && !request->url().startsWith("/api/")) {
      sendAdminUi(request);
      return;
    }
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });
  server_.begin();
}

void WebServer::setTxSink(TxSink sink, void* ctx) {
  txSink_ = sink;
  txCtx_ = ctx;
}

void WebServer::setConfigSaveHandler(ConfigSaveHandler handler, void* ctx) {
  saveHandler_ = handler;
  saveCtx_ = ctx;
}

void WebServer::setFactoryResetHandler(FactoryResetHandler handler, void* ctx) {
  resetHandler_ = handler;
  resetCtx_ = ctx;
}

void WebServer::updateCredentials(const SecurityConfig& security) {
  authMiddleware_.setUsername(security.username.c_str());
  authMiddleware_.setPassword(security.password.c_str());
  authMiddleware_.generateHash();
}

void WebServer::poll() {
  ws_.cleanupClients(2);
  ota_.poll();
}

void WebServer::broadcastPacket(const Packet& packet) {
  if (ws_.count() == 0) return;
  JsonDocument doc;
  doc["type"] = packetDirectionName(packet.direction);
  doc["ts"] = packet.timestampMs;
  doc["hex"] = packet.hex();
  doc["length"] = packet.length;
  if (packet.message.length()) doc["message"] = packet.message;
  ws_.textAll(jsonString(doc));
}

OtaManager& WebServer::ota() {
  return ota_;
}

void WebServer::sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int status) {
  request->send(status, "application/json", jsonString(doc));
}

void WebServer::sendError(AsyncWebServerRequest* request, int status, const char* code) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = code;
  sendJson(request, doc, status);
}

void WebServer::registerRoutes() {
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    status_->writeJson(doc, *config_);
    sendJson(request, doc);
  });

  server_.on("/api/info", HTTP_GET, [this](AsyncWebServerRequest* req) {
    handleInfo(req);
  });

  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    status_->writeJson(doc, *config_);
    sendJson(request, doc);
  });

  server_.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleWifiScan(request);
  });

  server_.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      collectBody(this, request, data, len, index, total, [](WebServer* self, AsyncWebServerRequest* req, const String& body) {
        self->handleConfigBody(req, body);
      });
    });

  server_.on("/api/tx", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      collectBody(this, request, data, len, index, total, [](WebServer* self, AsyncWebServerRequest* req, const String& body) {
        self->handleTxBody(req, body);
      });
    });

  server_.on("/api/scanner/result", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    scanner_->writeJson(doc);
    sendJson(request, doc);
  });

  server_.on("/api/scanner/start", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      collectBody(this, request, data, len, index, total, [](WebServer* self, AsyncWebServerRequest* req, const String& body) {
        self->handleScannerStartBody(req, body);
      });
    });

  server_.on("/api/scanner/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
    scanner_->stop();
    JsonDocument doc;
    scanner_->writeJson(doc);
    sendJson(request, doc);
  });

  server_.on("/api/reboot", HTTP_POST,
    [this](AsyncWebServerRequest* req) { handleReboot(req); });

  server_.on("/api/factory-reset/settings", HTTP_POST, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["data"]["reboot"] = true;
    doc["data"]["mode"] = "settings";
    sendJson(request, doc);
    if (resetHandler_) resetHandler_(resetCtx_, false);
  });

  server_.on("/api/factory-reset/all", HTTP_POST, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["data"]["reboot"] = true;
    doc["data"]["mode"] = "all";
    sendJson(request, doc);
    if (resetHandler_) resetHandler_(resetCtx_, true);
  });
}

void WebServer::handleConfigBody(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(request, 400, "invalid_json");
    return;
  }

  DeviceConfig next = *config_;
  if (doc["ssid"].is<const char*>()) next.wifi.ssid = doc["ssid"].as<String>();
  if (doc["wifi_password"].is<const char*>()) next.wifi.password = doc["wifi_password"].as<String>();
  if (doc["ap_password"].is<const char*>()) next.wifi.apPassword = doc["ap_password"].as<String>();
  if (doc["baud"].is<uint32_t>()) next.uart.baud = doc["baud"].as<uint32_t>();
  if (doc["data_bits"].is<uint8_t>()) next.uart.dataBits = doc["data_bits"].as<uint8_t>();
  if (doc["stop_bits"].is<uint8_t>()) next.uart.stopBits = doc["stop_bits"].as<uint8_t>();
  if (doc["parity"].is<const char*>()) next.uart.parity = doc["parity"].as<String>();
  if (doc["framing"].is<const char*>()) next.uart.framing = doc["framing"].as<String>();
  if (doc["idle_gap_ms"].is<uint16_t>()) next.uart.idleGapMs = doc["idle_gap_ms"].as<uint16_t>();
  if (doc["uart_rx_buffer"].is<uint16_t>()) next.uart.rxBufferBytes = doc["uart_rx_buffer"].as<uint16_t>();
  if (doc["tcp_mode"].is<const char*>()) next.tcp.mode = doc["tcp_mode"].as<String>();
  if (doc["tcp_host"].is<const char*>()) next.tcp.host = doc["tcp_host"].as<String>();
  if (doc["tcp_port"].is<uint16_t>()) next.tcp.port = doc["tcp_port"].as<uint16_t>();
  if (doc["max_clients"].is<uint8_t>()) next.tcp.maxClients = doc["max_clients"].as<uint8_t>();
  if (doc["console_limit"].is<uint16_t>()) next.console.lineLimit = doc["console_limit"].as<uint16_t>();
  if (doc["device_name"].is<const char*>()) next.deviceName = doc["device_name"].as<String>();
  if (doc["log_enabled"].is<bool>()) next.log.enabled = doc["log_enabled"].as<bool>();
  if (doc["log_port"].is<uint16_t>()) next.log.port = doc["log_port"].as<uint16_t>();
  if (doc["log_max_clients"].is<uint8_t>()) next.log.maxClients = doc["log_max_clients"].as<uint8_t>();
  if (doc["log_heartbeat_ms"].is<uint16_t>()) next.log.heartbeatMs = doc["log_heartbeat_ms"].as<uint16_t>();

  bool ok = store_->save(next);
  if (ok) {
    *config_ = next;
    updateCredentials(next.security);
    if (saveHandler_) saveHandler_(next, saveCtx_);
  }
  JsonDocument response;
  response["ok"] = ok;
  response["data"]["reboot"] = true;
  sendJson(request, response, ok ? 200 : 500);
}

void WebServer::handleTxBody(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    sendError(request, 400, "invalid_json");
    return;
  }
  String hex = doc["hex"].as<String>();
  HexParseResult parsed = parseHex(hex);
  if (!parsed.ok) {
    sendError(request, 400, parsed.error.c_str());
    return;
  }
  bool queued = txSink_ && txSink_(parsed.bytes, parsed.length, txCtx_);
  JsonDocument response;
  response["ok"] = queued;
  response["data"]["queued"] = queued;
  response["data"]["length"] = parsed.length;
  sendJson(request, response, queued ? 200 : 503);
}

void WebServer::handleScannerStartBody(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    sendError(request, 400, "invalid_json");
    return;
  }
  bool ok = scanner_->start(doc["start"] | 9600, doc["end"] | 115200, doc["step"] | 9600, doc["sample_ms"] | 500);
  JsonDocument response;
  if (ok) scanner_->writeJson(response);
  else {
    response["ok"] = false;
    response["error"] = "invalid_scan_range";
  }
  sendJson(request, response, ok ? 200 : 400);
}

void WebServer::handleWifiScan(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["ok"] = true;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonArray networks = data["networks"].to<JsonArray>();

  int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) {
    data["phase"] = "scanning";
    sendJson(request, doc);
    return;
  }

  if (result == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    int started = WiFi.scanNetworks(true, true);
    data["phase"] = started == WIFI_SCAN_RUNNING ? "scanning" : "failed";
    sendJson(request, doc, started == WIFI_SCAN_RUNNING ? 202 : 503);
    return;
  }

  data["phase"] = "done";
  data["count"] = result;
  const int limit = min(result, 20);
  for (int i = 0; i < limit; ++i) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["channel"] = WiFi.channel(i);
    item["encrypted"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(request, doc);
}

void WebServer::handleWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text("{\"type\":\"system\",\"message\":\"connected\"}");
    return;
  }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
  if (!info || !info->final || info->index != 0 || info->opcode != WS_TEXT) return;
  String text;
  text.reserve(len);
  for (size_t i = 0; i < len; ++i) text += static_cast<char>(data[i]);
  handleWsText(client, text);
}

void WebServer::handleWsText(AsyncWebSocketClient* client, const String& text) {
  JsonDocument doc;
  if (deserializeJson(doc, text)) {
    client->text("{\"type\":\"error\",\"message\":\"invalid_json\"}");
    return;
  }
  String type = doc["type"].as<String>();
  if (type == "ping") {
    client->text("{\"type\":\"pong\"}");
    return;
  }
  if (type == "tx") {
    String hex = doc["hex"].as<String>();
    HexParseResult parsed = parseHex(hex);
    if (!parsed.ok || !txSink_ || !txSink_(parsed.bytes, parsed.length, txCtx_)) {
      client->text("{\"type\":\"error\",\"message\":\"tx_rejected\"}");
      return;
    }
    client->text("{\"type\":\"system\",\"message\":\"tx_queued\"}");
  }
}

void WebServer::collectBody(WebServer* self, AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, BodyHandler handler) {
  if (index == 0) request->_tempObject = new String();
  String* body = static_cast<String*>(request->_tempObject);
  if (!body) return;
  body->reserve(total);
  for (size_t i = 0; i < len; ++i) *body += static_cast<char>(data[i]);
  if (index + len == total) {
    String complete = *body;
    delete body;
    request->_tempObject = nullptr;
    handler(self, request, complete);
  }
}

void WebServer::handleReboot(AsyncWebServerRequest* request) {
  rebootScheduledMs_ = millis() + kRebootDelayMs;
  JsonDocument doc;
  doc["queued"] = true;
  doc["scheduledMs"] = kRebootDelayMs;
  sendJson(request, doc);
}

void WebServer::pollRebootDeadline() {
  // Implemented in Task 6
}

void WebServer::handleInfo(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["ok"] = true;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonObject fw = data["firmware"].to<JsonObject>();
  fw["version"]  = "0.1.10";  // NOTE: version is intentionally duplicated with device_status.cpp; a shared FIRMWARE_VERSION macro is deferred to a follow-up refactor
  fw["commit"]   = BUILD_COMMIT;
  fw["built_at"] = BUILD_AT;
  fw["board"]    = ARDUINO_BOARD;
  JsonObject tcp = data["tcp"].to<JsonObject>();
  tcp["max_clients"] = config_->tcp.maxClients;
  JsonObject queue = data["queue"].to<JsonObject>();
  queue["capacity"] = config_->uart.rxBufferBytes;  // NOTE: maps to the UART RX buffer size as a practical proxy for queue capacity; revisit once PacketQueue exposes its own capacity
  sendJson(request, doc);
}

}  // namespace dm
