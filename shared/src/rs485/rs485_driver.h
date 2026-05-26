#pragma once

#include <Arduino.h>
#include "../status/device_status.h"
#include "../storage/config_store.h"
#include "packet_parser.h"
#include "packet_queue.h"

namespace dm {

class Rs485Driver {
 public:
  bool begin(HardwareSerial& serial, const DeviceConfig& config, PacketQueue& rxQueue, PacketQueue& txQueue, PacketQueue& txHistory, DeviceStatus& status);
  void reconfigure(const DeviceConfig& config);
  void poll();
  bool enqueueTx(const uint8_t* data, size_t length);
  bool busy() const;

 private:
  uint32_t serialMode(const UartConfig& config) const;
  void processRx();
  void processTx();
  void setTransmit(bool enabled);

  HardwareSerial* serial_ = nullptr;
  PacketQueue* rxQueue_ = nullptr;
  PacketQueue* txQueue_ = nullptr;
  PacketQueue* txHistory_ = nullptr;
  DeviceStatus* status_ = nullptr;
  DeviceConfig config_;
  PacketParser parser_;
  bool txBusy_ = false;
};

}  // namespace dm
