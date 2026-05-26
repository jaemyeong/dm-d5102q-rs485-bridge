#include "tcp_bridge.h"

namespace dm {

void TcpBridge::begin(const DeviceConfig& config, DeviceStatus& status) {
  config_ = config;
  status_ = &status;
  activePort_ = 0;
  restartServerIfNeeded();
}

void TcpBridge::setTxSink(TxSink sink, void* ctx) {
  txSink_ = sink;
  txCtx_ = ctx;
}

void TcpBridge::poll() {
  if (config_.tcp.mode == "client") {
    pollClient();
  } else {
    pollServer();
  }
}

void TcpBridge::broadcast(const Packet& packet) {
  if (packet.length == 0) return;
  if (config_.tcp.mode == "client") {
    if (outbound_.connected()) outbound_.write(packet.data, packet.length);
    return;
  }
  for (uint8_t i = 0; i < config_.tcp.maxClients && i < 4; ++i) {
    if (clients_[i] && clients_[i].connected()) {
      clients_[i].write(packet.data, packet.length);
    }
  }
}

uint8_t TcpBridge::clientCount() {
  if (config_.tcp.mode == "client") return outbound_.connected() ? 1 : 0;
  uint8_t count = 0;
  for (uint8_t i = 0; i < config_.tcp.maxClients && i < 4; ++i) {
    if (clients_[i] && clients_[i].connected()) count++;
  }
  return count;
}

void TcpBridge::stop() {
  for (WiFiClient& client : clients_) client.stop();
  outbound_.stop();
  server_.stop();
  activePort_ = 0;
}

void TcpBridge::pollServer() {
  restartServerIfNeeded();
  acceptClients();
  for (uint8_t i = 0; i < config_.tcp.maxClients && i < 4; ++i) {
    if (clients_[i] && clients_[i].connected()) {
      readClient(clients_[i]);
    } else if (clients_[i]) {
      clients_[i].stop();
    }
  }
}

void TcpBridge::pollClient() {
  if (!ensureClientModeConnection()) return;
  readClient(outbound_);
}

void TcpBridge::acceptClients() {
  WiFiClient incoming = server_.available();
  if (!incoming) return;

  for (uint8_t i = 0; i < config_.tcp.maxClients && i < 4; ++i) {
    if (!clients_[i] || !clients_[i].connected()) {
      clients_[i].stop();
      clients_[i] = incoming;
      clients_[i].setNoDelay(true);
      return;
    }
  }
  incoming.stop();
  if (status_) status_->recordDroppedPacket();
}

void TcpBridge::readClient(WiFiClient& client) {
  uint8_t buffer[128];
  while (client.connected() && client.available() > 0) {
    size_t toRead = static_cast<size_t>(client.available());
    if (toRead > sizeof(buffer)) toRead = sizeof(buffer);
    int bytesRead = client.read(buffer, toRead);
    if (bytesRead > 0 && txSink_ && !txSink_(buffer, static_cast<size_t>(bytesRead), txCtx_)) {
      if (status_) status_->recordQueueOverflow();
    }
  }
}

bool TcpBridge::ensureClientModeConnection() {
  if (outbound_.connected()) return true;
  if (millis() - lastConnectAttemptMs_ < config_.tcp.reconnectBackoffMs) return false;
  lastConnectAttemptMs_ = millis();
  outbound_.stop();
  if (config_.tcp.host.length() == 0 || config_.tcp.host == "0.0.0.0") return false;
  outbound_.connect(config_.tcp.host.c_str(), config_.tcp.port);
  outbound_.setNoDelay(true);
  return outbound_.connected();
}

void TcpBridge::restartServerIfNeeded() {
  if (config_.tcp.mode != "server") return;
  if (activePort_ == config_.tcp.port) return;
  server_.stop();
  server_.begin(config_.tcp.port);
  server_.setNoDelay(true);
  activePort_ = config_.tcp.port;
}

}  // namespace dm
