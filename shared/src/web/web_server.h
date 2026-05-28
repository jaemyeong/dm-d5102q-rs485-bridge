#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "../network/tcp_bridge.h"
#include "../rs485/packet.h"
#include "../scanner/baud_scanner.h"
#include "../status/device_status.h"
#include "../storage/config_store.h"
#include "ota_manager.h"

namespace dm {

using ConfigSaveHandler = bool (*)(const DeviceConfig& config, void* ctx);
using FactoryResetHandler = void (*)(void* ctx, bool fullWipe);

class WebServer {
 public:
  void begin(DeviceConfig& config, ConfigStore& store, DeviceStatus& status, BaudScanner& scanner);
  void setTxSink(TxSink sink, void* ctx);
  void setConfigSaveHandler(ConfigSaveHandler handler, void* ctx);
  void setFactoryResetHandler(FactoryResetHandler handler, void* ctx);
  void poll();
  void pollRebootDeadline();
  void broadcastPacket(const Packet& packet);
  OtaManager& ota();

 private:
  using BodyHandler = void (*)(WebServer*, AsyncWebServerRequest*, const String&);

  void updateCredentials(const SecurityConfig& security);
  void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int status = 200);
  void sendError(AsyncWebServerRequest* request, int status, const char* code);
  void registerRoutes();
  void handleConfigBody(AsyncWebServerRequest* request, const String& body);
  void handleTxBody(AsyncWebServerRequest* request, const String& body);
  void handleScannerStartBody(AsyncWebServerRequest* request, const String& body);
  void handleWifiScan(AsyncWebServerRequest* request);
  void handleInfo(AsyncWebServerRequest* request);
  void handleReboot(AsyncWebServerRequest* request);
  void handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleWsText(AsyncWebSocketClient* client, const String& text);
  static void collectBody(WebServer* self, AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, BodyHandler handler);

  AsyncAuthenticationMiddleware authMiddleware_;
  AsyncWebServer server_{80};
  AsyncWebSocket ws_{"/ws/console"};
  DeviceConfig* config_ = nullptr;
  ConfigStore* store_ = nullptr;
  DeviceStatus* status_ = nullptr;
  BaudScanner* scanner_ = nullptr;
  TxSink txSink_ = nullptr;
  void* txCtx_ = nullptr;
  ConfigSaveHandler saveHandler_ = nullptr;
  void* saveCtx_ = nullptr;
  FactoryResetHandler resetHandler_ = nullptr;
  void* resetCtx_ = nullptr;
  uint32_t rebootScheduledMs_ = 0;
  static constexpr uint32_t kRebootDelayMs = 500;
  OtaManager ota_;
};

}  // namespace dm
