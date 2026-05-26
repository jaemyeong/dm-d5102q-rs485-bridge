#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "../status/device_status.h"

namespace dm {

class OtaManager {
 public:
  void begin(AsyncWebServer& server, DeviceStatus& status);
  bool rebootPending() const;
  void poll();

 private:
  bool rebootPending_ = false;
  uint32_t rebootAtMs_ = 0;
};

}  // namespace dm
