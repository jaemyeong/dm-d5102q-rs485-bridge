#include "log_server.h"

#include <stdarg.h>

namespace dm {

void LogServer::begin(uint16_t port, uint8_t maxClients) {
  if (maxClients < 1) maxClients = 1;
  if (maxClients > kHardCap) maxClients = kHardCap;
  if (started_ && activePort_ == port && maxClients_ == maxClients) return;
  if (started_) stop();
  maxClients_ = maxClients;
  activePort_ = port;
  server_.stop();
  server_.begin(port);
  server_.setNoDelay(true);
  started_ = true;
}

void LogServer::stop() {
  for (WiFiClient& c : clients_) c.stop();
  server_.stop();
  activePort_ = 0;
  started_ = false;
}

void LogServer::poll() {
  if (!started_) return;
  acceptClients();
  reapClients();
}

uint8_t LogServer::clientCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < maxClients_; i++) {
    if (clients_[i] && clients_[i].connected()) n++;
  }
  return n;
}

void LogServer::log(const char* fmt, ...) {
  char buf[320];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len <= 0) return;
  if (len >= static_cast<int>(sizeof(buf))) len = sizeof(buf) - 1;
  if (len == 0 || buf[len - 1] != '\n') {
    if (len < static_cast<int>(sizeof(buf)) - 1) {
      buf[len++] = '\n';
      buf[len] = '\0';
    }
  }
  write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(len));
}

void LogServer::write(const uint8_t* data, size_t length) {
  Serial.write(data, length);
  if (!started_) return;
  for (uint8_t i = 0; i < maxClients_; i++) {
    if (clients_[i] && clients_[i].connected()) {
      clients_[i].write(data, length);
    }
  }
}

void LogServer::acceptClients() {
  WiFiClient incoming = server_.available();
  if (!incoming) return;
  for (uint8_t i = 0; i < maxClients_; i++) {
    if (!clients_[i] || !clients_[i].connected()) {
      clients_[i].stop();
      clients_[i] = incoming;
      clients_[i].setNoDelay(true);
      char buf[96];
      int n = snprintf(buf, sizeof(buf),
                       "[log] connected slot=%u peer=%s\n", i,
                       clients_[i].remoteIP().toString().c_str());
      if (n > 0) write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
      return;
    }
  }
  incoming.stop();
}

void LogServer::reapClients() {
  for (uint8_t i = 0; i < maxClients_; i++) {
    if (clients_[i] && !clients_[i].connected()) {
      clients_[i].stop();
    }
  }
}

}  // namespace dm
