#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../storage/config_store.h"

namespace dm {

struct RuntimeMetrics {
  uint32_t bootMs = 0;
  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  uint32_t rxBytes = 0;
  uint32_t txBytes = 0;
  uint32_t uartOverflow = 0;
  uint32_t queueOverflow = 0;
  uint32_t droppedPackets = 0;
  uint16_t queueUsage = 0;
  uint8_t tcpClients = 0;
  int32_t rssi = 0;
  uint32_t lastRxMs = 0;
  uint32_t lastTxMs = 0;
};

class DeviceStatus {
 public:
  void begin();
  void tick(const DeviceConfig& config, uint8_t tcpClients, uint16_t queueUsage);
  void recordRx(size_t bytes);
  void recordTx(size_t bytes);
  void recordUartOverflow();
  void recordQueueOverflow();
  void recordDroppedPacket();
  RuntimeMetrics metrics() const;
  void writeJson(JsonDocument& doc, const DeviceConfig& config) const;

 private:
  RuntimeMetrics metrics_;
};

}  // namespace dm
