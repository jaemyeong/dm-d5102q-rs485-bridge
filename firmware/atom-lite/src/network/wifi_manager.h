#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "../storage/config_store.h"

namespace dm {

enum class WifiState : uint8_t {
  Idle,
  Connecting,
  Connected,
  AccessPoint
};

class BridgeWifiManager {
 public:
  void begin(const DeviceConfig& config, bool forceAp = false);
  void poll();
  WifiState state() const;
  bool apMode() const;
  String ipAddress() const;
  int32_t rssi() const;

 private:
  void startStation();
  void startAccessPoint();

  DeviceConfig config_;
  WifiState state_ = WifiState::Idle;
  uint32_t startedAtMs_ = 0;
  uint32_t lastRetryMs_ = 0;
};

}  // namespace dm
