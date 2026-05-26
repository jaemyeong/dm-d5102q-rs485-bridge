#include "wifi_manager.h"

namespace dm {

void BridgeWifiManager::begin(const DeviceConfig& config, bool forceAp) {
  config_ = config;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  if (forceAp || config_.wifi.ssid.length() == 0) {
    startAccessPoint();
  } else {
    startStation();
  }
}

void BridgeWifiManager::poll() {
  if (state_ == WifiState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      state_ = WifiState::Connected;
      return;
    }
    if (millis() - startedAtMs_ > config_.wifi.connectTimeoutMs) {
      startAccessPoint();
    }
    return;
  }

  if (state_ == WifiState::Connected && WiFi.status() != WL_CONNECTED) {
    state_ = WifiState::Connecting;
    lastRetryMs_ = millis();
  }

  if (state_ == WifiState::Connecting && millis() - lastRetryMs_ > config_.wifi.retryBackoffMs) {
    lastRetryMs_ = millis();
    WiFi.disconnect();
    WiFi.begin(config_.wifi.ssid.c_str(), config_.wifi.password.c_str());
  }
}

WifiState BridgeWifiManager::state() const {
  return state_;
}

bool BridgeWifiManager::apMode() const {
  return state_ == WifiState::AccessPoint;
}

String BridgeWifiManager::ipAddress() const {
  if (WiFi.isConnected()) return WiFi.localIP().toString();
  return WiFi.softAPIP().toString();
}

int32_t BridgeWifiManager::rssi() const {
  return WiFi.isConnected() ? WiFi.RSSI() : 0;
}

void BridgeWifiManager::startStation() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config_.wifi.ssid.c_str(), config_.wifi.password.c_str());
  startedAtMs_ = millis();
  lastRetryMs_ = startedAtMs_;
  state_ = WifiState::Connecting;
}

void BridgeWifiManager::startAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  if (config_.wifi.apPassword.length() >= 8) {
    WiFi.softAP(config_.wifi.apSsid.c_str(), config_.wifi.apPassword.c_str());
  } else {
    WiFi.softAP(config_.wifi.apSsid.c_str());
  }
  state_ = WifiState::AccessPoint;
}

}  // namespace dm
