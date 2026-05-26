#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace dm {

class LogServer {
 public:
  static constexpr uint8_t kHardCap = 4;

  void begin(uint16_t port, uint8_t maxClients);
  void stop();
  void poll();

  bool started() const { return started_; }
  uint16_t port() const { return activePort_; }
  uint8_t clientCount();

  void log(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  void write(const uint8_t* data, size_t length);

 private:
  void acceptClients();
  void reapClients();

  bool started_ = false;
  uint16_t activePort_ = 0;
  uint8_t maxClients_ = 2;
  WiFiServer server_{0};
  WiFiClient clients_[kHardCap];
};

}  // namespace dm
