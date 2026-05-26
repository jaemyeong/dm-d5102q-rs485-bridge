#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "../rs485/packet.h"
#include "../status/device_status.h"
#include "../storage/config_store.h"

namespace dm {

using TxSink = bool (*)(const uint8_t* data, size_t length, void* ctx);

class TcpBridge {
 public:
  void begin(const DeviceConfig& config, DeviceStatus& status);
  void setTxSink(TxSink sink, void* ctx);
  void poll();
  void broadcast(const Packet& packet);
  uint8_t clientCount() const;
  void stop();

 private:
  void pollServer();
  void pollClient();
  void acceptClients();
  void readClient(WiFiClient& client);
  bool ensureClientModeConnection();
  void restartServerIfNeeded();

  DeviceConfig config_;
  DeviceStatus* status_ = nullptr;
  WiFiServer server_{502};
  WiFiClient clients_[4];
  WiFiClient outbound_;
  TxSink txSink_ = nullptr;
  void* txCtx_ = nullptr;
  uint16_t activePort_ = 0;
  uint32_t lastConnectAttemptMs_ = 0;
};

}  // namespace dm
