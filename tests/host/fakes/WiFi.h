#pragma once
#include "Arduino.h"
#include <unistd.h>

constexpr int WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WL_CONNECTED = 3;
struct IPAddress {
  uint32_t value;
  IPAddress(uint32_t address = 1) : value(address) {}
  String toString() const { return "192.168.4.1"; }
  unsigned operator[](size_t index) const { const unsigned bytes[] = {192, 168, 4, 1}; return bytes[index]; }
  explicit operator uint32_t() const { return value; }
};
struct FakeWiFi {
  int currentMode = WIFI_OFF;
  bool linked = false;
  unsigned attempts = 0;
  void persistent(bool) {}
  void setAutoReconnect(bool) {}
  bool mode(int value) { currentMode = value; return true; }
  int getMode() { return currentMode; }
  void setHostname(const char*) {}
  void disconnect(bool, bool) { linked = false; }
  bool softAP(const char*, const char*, unsigned, bool, unsigned) { return true; }
  IPAddress softAPIP() { return IPAddress(); }
  IPAddress localIP() { return IPAddress(linked ? 1 : 0); }
  void begin(const char*, const char*) { ++attempts; }
  int status() { return linked ? WL_CONNECTED : 0; }
};
extern FakeWiFi WiFi;
class WiFiClient {
  int descriptor_ = -1;
 public:
  WiFiClient() = default;
  explicit WiFiClient(int descriptor) : descriptor_(descriptor) {}
  int fd() const { return descriptor_; }
  void stop() { if (descriptor_ >= 0) ::close(descriptor_); descriptor_ = -1; }
  IPAddress localIP() const { return IPAddress(); }
};
class WiFiServer {
 public:
  int accepted = -1;
  bool listening = false;
  WiFiServer(unsigned, unsigned) {}
  void begin() { listening = true; }
  void end() { listening = false; }
  explicit operator bool() const { return listening; }
  WiFiClient available() { const int fd = accepted; accepted = -1; return WiFiClient(fd); }
};
