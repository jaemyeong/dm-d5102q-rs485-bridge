#include "secrets.h"
#include "board_config.h"
#include "src/app/firmware_app.h"

dm::FirmwareApp app;

void setup() {
  app.begin();
}

void loop() {
  app.loop();
}
