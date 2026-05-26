#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace dm {

struct WifiConfig {
  String ssid;
  String password;
  String apSsid = "DM-D5102Q-setup";
  String apPassword = "dm5102q123";
  uint32_t connectTimeoutMs = 15000;
  uint32_t retryBackoffMs = 5000;
};

struct UartConfig {
  uint32_t baud = 3840;
  uint8_t dataBits = 8;
  uint8_t stopBits = 1;
  String parity = "none";
  String framing = "inter-byte";
  uint16_t idleGapMs = 20;
  uint16_t rxBufferBytes = 2048;
};

struct TcpConfig {
  String mode = "server";
  String host = "0.0.0.0";
  uint16_t port = 502;
  uint8_t maxClients = 2;
  uint32_t reconnectBackoffMs = 3000;
  bool keepAlive = true;
};

struct ConsoleConfig {
  uint16_t lineLimit = 300;
  uint16_t replayPackets = 64;
  bool confirmTx = false;
};

struct SecurityConfig {
  bool basicAuth = true;
  String username = "admin";
  String password = "admin";
};

struct LogServerConfig {
  bool enabled = true;
  uint16_t port = 8898;
  uint8_t maxClients = 2;
  uint16_t heartbeatMs = 2000;
};

struct DeviceConfig {
  String deviceName = "bridge";
  WifiConfig wifi;
  UartConfig uart;
  TcpConfig tcp;
  ConsoleConfig console;
  SecurityConfig security;
  LogServerConfig log;
};

class ConfigStore {
 public:
  bool begin();
  void end();
  DeviceConfig load();
  bool save(const DeviceConfig& config);
  bool resetExceptWifi();
  bool eraseAll();
  bool hasWifiCredentials(const DeviceConfig& config) const;

 private:
  DeviceConfig defaults() const;
  void validate(DeviceConfig& config) const;

  Preferences prefs_;
  bool open_ = false;
};

}  // namespace dm
