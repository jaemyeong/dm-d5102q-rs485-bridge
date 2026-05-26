#include "ota_manager.h"

#include <Update.h>

namespace dm {

void OtaManager::begin(AsyncWebServer& server, DeviceStatus&) {
  server.on("/update", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      rebootPending_ = !Update.hasError();
      AsyncWebServerResponse* response = request->beginResponse(rebootPending_ ? 200 : 500, "text/plain", rebootPending_ ? "OK" : "FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
          Update.printError(Serial);
        }
      }
      if (!Update.hasError() && Update.write(data, len) != len) {
        Update.printError(Serial);
      }
      if (final && !Update.end(true)) {
        Update.printError(Serial);
      }
    }
  );
}

bool OtaManager::rebootPending() const {
  return rebootPending_;
}

void OtaManager::poll() {
  if (!rebootPending_) return;
  delay(100);
  ESP.restart();
}

}  // namespace dm
