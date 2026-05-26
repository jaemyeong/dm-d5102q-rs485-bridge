#include "ota_manager.h"

#include <Update.h>

namespace dm {

void OtaManager::begin(AsyncWebServer& server, DeviceStatus&) {
  server.on("/update", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      updating_ = false;
      rebootPending_ = !Update.hasError();
      if (rebootPending_) rebootAtMs_ = millis() + 250;
      AsyncWebServerResponse* response = request->beginResponse(rebootPending_ ? 200 : 500, "text/plain", rebootPending_ ? "OK firmware" : "FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
    },
    [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      (void)request;
      (void)filename;
      if (index == 0) {
        updating_ = true;
        rebootPending_ = false;
        if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FLASH)) {
          Update.printError(Serial);
        }
      }
      if (!Update.hasError() && Update.write(data, len) != len) {
        Update.printError(Serial);
      }
      if (final && !Update.end(true)) {
        Update.printError(Serial);
      }
      if (final) updating_ = false;
    }
  );
}

bool OtaManager::rebootPending() const {
  return rebootPending_;
}

bool OtaManager::updating() const {
  return updating_;
}

void OtaManager::poll() {
  if (!rebootPending_) return;
  if (millis() < rebootAtMs_) return;
  ESP.restart();
}

}  // namespace dm
