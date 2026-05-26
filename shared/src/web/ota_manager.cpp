#include "ota_manager.h"

#include <LittleFS.h>
#include <Update.h>

namespace dm {

namespace {

struct OtaUploadState {
  bool littlefs = false;
};

bool isLittleFsImage(const String& filename) {
  String lower = filename;
  lower.toLowerCase();
  return lower.indexOf("littlefs") >= 0 || lower.indexOf("webui") >= 0 || lower.indexOf("filesystem") >= 0;
}

}  // namespace

void OtaManager::begin(AsyncWebServer& server, DeviceStatus&) {
  server.on("/update", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      OtaUploadState* state = static_cast<OtaUploadState*>(request->_tempObject);
      rebootPending_ = !Update.hasError();
      if (rebootPending_) rebootAtMs_ = millis() + 250;
      String body = rebootPending_ ? (state && state->littlefs ? "OK littlefs" : "OK firmware") : "FAIL";
      AsyncWebServerResponse* response = request->beginResponse(rebootPending_ ? 200 : 500, "text/plain", body);
      response->addHeader("Connection", "close");
      request->send(response);
      delete state;
      request->_tempObject = nullptr;
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        OtaUploadState* state = new OtaUploadState();
        state->littlefs = isLittleFsImage(filename);
        request->_tempObject = state;
        bool started = false;
        if (state->littlefs) {
          LittleFS.end();
          started = Update.begin(UPDATE_SIZE_UNKNOWN, U_LITTLEFS, -1, LOW, "littlefs");
        } else {
          started = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FLASH);
        }
        if (!started) {
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
  if (millis() < rebootAtMs_) return;
  ESP.restart();
}

}  // namespace dm
