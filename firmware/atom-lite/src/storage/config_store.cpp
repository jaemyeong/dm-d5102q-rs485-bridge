#include "config_store.h"

#include "board_config.h"

namespace dm {

bool ConfigStore::begin() {
  open_ = prefs_.begin("bridge", false);
  return open_;
}

void ConfigStore::end() {
  if (open_) prefs_.end();
  open_ = false;
}

DeviceConfig ConfigStore::load() {
  DeviceConfig config = defaults();
  if (!open_) return config;

  config.deviceName = prefs_.getString("dev_name", config.deviceName);
  config.wifi.ssid = prefs_.getString("wifi_ssid", config.wifi.ssid);
  config.wifi.password = prefs_.getString("wifi_psk", config.wifi.password);
  config.wifi.apSsid = prefs_.getString("ap_ssid", config.wifi.apSsid);
  config.wifi.apPassword = prefs_.getString("ap_psk", config.wifi.apPassword);
  config.wifi.connectTimeoutMs = prefs_.getUInt("wifi_timeout", config.wifi.connectTimeoutMs);
  config.uart.baud = prefs_.getUInt("uart_baud", config.uart.baud);
  config.uart.dataBits = prefs_.getUChar("uart_data", config.uart.dataBits);
  config.uart.stopBits = prefs_.getUChar("uart_stop", config.uart.stopBits);
  config.uart.parity = prefs_.getString("uart_parity", config.uart.parity);
  config.uart.framing = prefs_.getString("framing", config.uart.framing);
  config.uart.idleGapMs = prefs_.getUShort("idle_gap", config.uart.idleGapMs);
  config.tcp.mode = prefs_.getString("tcp_mode", config.tcp.mode);
  config.tcp.host = prefs_.getString("tcp_host", config.tcp.host);
  config.tcp.port = prefs_.getUShort("tcp_port", config.tcp.port);
  config.tcp.maxClients = prefs_.getUChar("tcp_clients", config.tcp.maxClients);
  config.console.lineLimit = prefs_.getUShort("con_lines", config.console.lineLimit);
  config.console.replayPackets = prefs_.getUShort("replay", config.console.replayPackets);
  config.console.confirmTx = prefs_.getBool("confirm_tx", config.console.confirmTx);
  config.security.basicAuth = prefs_.getBool("basic_auth", config.security.basicAuth);
  config.security.username = prefs_.getString("auth_user", config.security.username);
  config.security.password = prefs_.getString("auth_pass", config.security.password);

  validate(config);
  return config;
}

bool ConfigStore::save(const DeviceConfig& input) {
  if (!open_) return false;
  DeviceConfig config = input;
  validate(config);

  bool ok = true;
  ok &= prefs_.putString("dev_name", config.deviceName) > 0;
  prefs_.putString("wifi_ssid", config.wifi.ssid);
  prefs_.putString("wifi_psk", config.wifi.password);
  prefs_.putString("ap_ssid", config.wifi.apSsid);
  prefs_.putString("ap_psk", config.wifi.apPassword);
  prefs_.putUInt("wifi_timeout", config.wifi.connectTimeoutMs);
  prefs_.putUInt("uart_baud", config.uart.baud);
  prefs_.putUChar("uart_data", config.uart.dataBits);
  prefs_.putUChar("uart_stop", config.uart.stopBits);
  prefs_.putString("uart_parity", config.uart.parity);
  prefs_.putString("framing", config.uart.framing);
  prefs_.putUShort("idle_gap", config.uart.idleGapMs);
  prefs_.putString("tcp_mode", config.tcp.mode);
  prefs_.putString("tcp_host", config.tcp.host);
  prefs_.putUShort("tcp_port", config.tcp.port);
  prefs_.putUChar("tcp_clients", config.tcp.maxClients);
  prefs_.putUShort("con_lines", config.console.lineLimit);
  prefs_.putUShort("replay", config.console.replayPackets);
  prefs_.putBool("confirm_tx", config.console.confirmTx);
  prefs_.putBool("basic_auth", config.security.basicAuth);
  prefs_.putString("auth_user", config.security.username);
  prefs_.putString("auth_pass", config.security.password);
  return ok;
}

bool ConfigStore::reset() {
  return open_ && prefs_.clear();
}

bool ConfigStore::hasWifiCredentials(const DeviceConfig& config) const {
  return config.wifi.ssid.length() > 0;
}

DeviceConfig ConfigStore::defaults() const {
  DeviceConfig config;
#ifdef BRIDGE_DEFAULT_DEVICE_NAME
  config.deviceName = BRIDGE_DEFAULT_DEVICE_NAME;
#endif
#ifdef BRIDGE_DEFAULT_AP_SSID
  config.wifi.apSsid = BRIDGE_DEFAULT_AP_SSID;
#endif
#ifdef BRIDGE_DEFAULT_AP_PASSWORD
  config.wifi.apPassword = BRIDGE_DEFAULT_AP_PASSWORD;
#endif
#ifdef BRIDGE_DEFAULT_WEB_USER
  config.security.username = BRIDGE_DEFAULT_WEB_USER;
#endif
#ifdef BRIDGE_DEFAULT_WEB_PASSWORD
  config.security.password = BRIDGE_DEFAULT_WEB_PASSWORD;
#endif
  return config;
}

void ConfigStore::validate(DeviceConfig& config) const {
  config.deviceName.trim();
  if (config.deviceName.length() == 0) config.deviceName = "bridge";
  if (config.uart.baud < 300 || config.uart.baud > 921600) config.uart.baud = 3840;
  if (config.uart.dataBits != 7 && config.uart.dataBits != 8) config.uart.dataBits = 8;
  if (config.uart.stopBits != 1 && config.uart.stopBits != 2) config.uart.stopBits = 1;
  if (config.uart.parity != "none" && config.uart.parity != "even" && config.uart.parity != "odd") config.uart.parity = "none";
  if (config.uart.framing != "inter-byte" && config.uart.framing != "delimiter" && config.uart.framing != "length") config.uart.framing = "inter-byte";
  if (config.uart.idleGapMs == 0 || config.uart.idleGapMs > 1000) config.uart.idleGapMs = 20;
  if (config.tcp.mode != "server" && config.tcp.mode != "client") config.tcp.mode = "server";
  if (config.tcp.port == 0) config.tcp.port = 502;
  config.tcp.maxClients = constrain(config.tcp.maxClients, static_cast<uint8_t>(1), static_cast<uint8_t>(4));
  config.console.lineLimit = constrain(config.console.lineLimit, static_cast<uint16_t>(50), static_cast<uint16_t>(500));
  config.console.replayPackets = constrain(config.console.replayPackets, static_cast<uint16_t>(8), static_cast<uint16_t>(128));
  if (config.security.username.length() == 0) config.security.username = "admin";
  if (config.security.password.length() == 0) config.security.password = "admin";
}

}  // namespace dm
