#include "device_status.h"

#include <ESP.h>
#include <WiFi.h>
#include "../utils/time.h"

namespace dm {

void DeviceStatus::begin() {
  metrics_ = RuntimeMetrics();
  metrics_.bootMs = millis();
}

void DeviceStatus::tick(const DeviceConfig&, uint8_t tcpClients, uint16_t queueUsage) {
  metrics_.tcpClients = tcpClients;
  metrics_.queueUsage = queueUsage;
  metrics_.rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
}

void DeviceStatus::recordRx(size_t bytes) {
  metrics_.rxPackets++;
  metrics_.rxBytes += bytes;
  metrics_.lastRxMs = millis();
}

void DeviceStatus::recordTx(size_t bytes) {
  metrics_.txPackets++;
  metrics_.txBytes += bytes;
  metrics_.lastTxMs = millis();
}

void DeviceStatus::recordUartOverflow() {
  metrics_.uartOverflow++;
  metrics_.droppedPackets++;
}

void DeviceStatus::recordQueueOverflow() {
  metrics_.queueOverflow++;
  metrics_.droppedPackets++;
}

void DeviceStatus::recordDroppedPacket() {
  metrics_.droppedPackets++;
}

RuntimeMetrics DeviceStatus::metrics() const {
  return metrics_;
}

void DeviceStatus::writeJson(JsonDocument& doc, const DeviceConfig& config) const {
  doc["ok"] = true;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonObject wifi = data["wifi"].to<JsonObject>();
  const bool connected = WiFi.isConnected();
  wifi["ssid"] = config.wifi.ssid;
  wifi["ip"] = connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  wifi["mac"] = connected ? WiFi.macAddress() : WiFi.softAPmacAddress();
  wifi["rssi"] = metrics_.rssi;
  wifi["connected"] = connected;
  wifi["mode"] = connected ? "sta" : "ap";

  JsonObject uart = data["uart"].to<JsonObject>();
  uart["baud"] = config.uart.baud;
  uart["data_bits"] = config.uart.dataBits;
  uart["stop_bits"] = config.uart.stopBits;
  uart["parity"] = config.uart.parity;
  uart["framing"] = config.uart.framing;
  uart["idle_gap_ms"] = config.uart.idleGapMs;

  JsonObject tcp = data["tcp"].to<JsonObject>();
  tcp["mode"] = config.tcp.mode;
  tcp["host"] = config.tcp.host;
  tcp["port"] = config.tcp.port;
  tcp["clients"] = metrics_.tcpClients;
  tcp["max_clients"] = config.tcp.maxClients;
  tcp["connected"] = metrics_.tcpClients > 0;

  JsonObject device = data["device"].to<JsonObject>();
  device["name"] = config.deviceName;
  device["board"] = ARDUINO_BOARD;
  device["version"] = "0.1.2";
  device["build"] = __DATE__ " " __TIME__;

  JsonObject console = data["console"].to<JsonObject>();
  console["limit"] = config.console.lineLimit;
  console["replay_packets"] = config.console.replayPackets;
  console["confirm_tx"] = config.console.confirmTx;

  JsonObject security = data["security"].to<JsonObject>();
  security["basic_auth"] = config.security.basicAuth;
  security["username"] = config.security.username;

  JsonObject metrics = data["metrics"].to<JsonObject>();
  metrics["uptime_sec"] = (millis() - metrics_.bootMs) / 1000UL;
  metrics["uptime"] = uptimeString((millis() - metrics_.bootMs) / 1000UL);
  metrics["heap_free"] = ESP.getFreeHeap();
  metrics["heap_total"] = ESP.getHeapSize();
  metrics["rx_packets"] = metrics_.rxPackets;
  metrics["tx_packets"] = metrics_.txPackets;
  metrics["rx_count"] = metrics_.rxPackets;
  metrics["tx_count"] = metrics_.txPackets;
  metrics["rx_bytes"] = metrics_.rxBytes;
  metrics["tx_bytes"] = metrics_.txBytes;
  metrics["queue_usage"] = metrics_.queueUsage;
  metrics["uart_overflow"] = metrics_.uartOverflow;
  metrics["queue_overflow"] = metrics_.queueOverflow;
  metrics["dropped_packets"] = metrics_.droppedPackets;
}

}  // namespace dm
