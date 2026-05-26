#pragma once

#include <Arduino.h>
#include "../network/tcp_bridge.h"
#include "../network/wifi_manager.h"
#include "../rs485/packet_queue.h"
#include "../rs485/rs485_driver.h"
#include "../scanner/baud_scanner.h"
#include "../status/device_status.h"
#include "../storage/config_store.h"
#include "../web/web_server.h"
#include "scheduler.h"

namespace dm {

class FirmwareApp {
 public:
  void begin();
  void loop();
  bool queueTx(const uint8_t* data, size_t length);
  void factoryReset();

 private:
  static bool txSink(const uint8_t* data, size_t length, void* ctx);
  static bool saveConfigSink(const DeviceConfig& config, void* ctx);
  static void factoryResetSink(void* ctx);
  static void tickStatusTask(void* ctx);

  void checkFactoryResetButton();
  void processPackets();
  void tickStatus();
  void applyConfig(const DeviceConfig& config);

  ConfigStore configStore_;
  DeviceConfig config_;
  DeviceStatus status_;
  BridgeWifiManager wifi_;
  PacketQueue rxQueue_{48};
  PacketQueue txQueue_{32};
  PacketQueue txHistory_{32};
  PacketQueue replay_{64};
  Rs485Driver rs485_;
  TcpBridge tcp_;
  BaudScanner scanner_;
  WebServer web_;
  Scheduler scheduler_;
  uint32_t resetHoldStartMs_ = 0;
  bool resetTriggered_ = false;
};

}  // namespace dm
