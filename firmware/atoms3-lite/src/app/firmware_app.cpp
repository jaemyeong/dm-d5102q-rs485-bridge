#include "firmware_app.h"

#include "board_config.h"

namespace dm {

void FirmwareApp::begin() {
  Serial.begin(115200);
  delay(50);

  configStore_.begin();
  config_ = configStore_.load();
  status_.begin();
  statusLed_.begin();
  statusLed_.setRgb(18, 18, 18);

  pinMode(FACTORY_RESET_BTN_PIN, INPUT_PULLUP);
  checkFactoryResetButton();

  bool forceAp = resetTriggered_ || !configStore_.hasWifiCredentials(config_);
  wifi_.begin(config_, forceAp);
  rs485_.begin(Serial2, config_, rxQueue_, txQueue_, txHistory_, status_);
  tcp_.begin(config_, status_);
  tcp_.setTxSink(&FirmwareApp::txSink, this);
  web_.begin(config_, configStore_, status_, scanner_);
  web_.setTxSink(&FirmwareApp::txSink, this);
  web_.setConfigSaveHandler(&FirmwareApp::saveConfigSink, this);
  web_.setFactoryResetHandler(&FirmwareApp::factoryResetSink, this);
  scheduler_.every(1000, &FirmwareApp::tickStatusTask, this);
}

void FirmwareApp::loop() {
  checkFactoryResetButton();
  wifi_.poll();
  rs485_.poll();
  tcp_.poll();
  scanner_.poll();
  web_.poll();
  processPackets();
  updateStatusLed();
  scheduler_.poll();
}

bool FirmwareApp::queueTx(const uint8_t* data, size_t length) {
  return rs485_.enqueueTx(data, length);
}

void FirmwareApp::factoryReset() {
  configStore_.reset();
  delay(150);
  ESP.restart();
}

bool FirmwareApp::txSink(const uint8_t* data, size_t length, void* ctx) {
  return static_cast<FirmwareApp*>(ctx)->queueTx(data, length);
}

bool FirmwareApp::saveConfigSink(const DeviceConfig& config, void* ctx) {
  static_cast<FirmwareApp*>(ctx)->applyConfig(config);
  return true;
}

void FirmwareApp::factoryResetSink(void* ctx) {
  static_cast<FirmwareApp*>(ctx)->factoryReset();
}

void FirmwareApp::tickStatusTask(void* ctx) {
  static_cast<FirmwareApp*>(ctx)->tickStatus();
}

void FirmwareApp::checkFactoryResetButton() {
  bool pressed = digitalRead(FACTORY_RESET_BTN_PIN) == LOW;
  if (!pressed) {
    resetHoldStartMs_ = 0;
    return;
  }
  if (resetHoldStartMs_ == 0) resetHoldStartMs_ = millis();
  if (!resetTriggered_ && millis() - resetHoldStartMs_ >= 5000) {
    resetTriggered_ = true;
    factoryReset();
  }
}

void FirmwareApp::processPackets() {
  Packet packet;
  while (rxQueue_.pop(packet)) {
    replay_.push(packet);
    tcp_.broadcast(packet);
    web_.broadcastPacket(packet);
  }
  while (txHistory_.pop(packet)) {
    replay_.push(packet);
    web_.broadcastPacket(packet);
  }
}

void FirmwareApp::tickStatus() {
  uint16_t queueUsage = 0;
  if (rxQueue_.capacity() > 0) {
    queueUsage = static_cast<uint16_t>((rxQueue_.size() * 100UL) / rxQueue_.capacity());
  }
  status_.tick(config_, tcp_.clientCount(), queueUsage);
}

void FirmwareApp::updateStatusLed() {
  const uint32_t now = millis();
  auto blink = [now](uint16_t intervalMs) {
    return ((now / intervalMs) % 2U) == 0U;
  };

  if (resetHoldStartMs_ != 0) {
    blink(125) ? statusLed_.setRgb(255, 0, 0) : statusLed_.off();
    return;
  }

  if (web_.ota().updating() || web_.ota().rebootPending()) {
    blink(180) ? statusLed_.setRgb(140, 0, 255) : statusLed_.off();
    return;
  }

  const RuntimeMetrics metrics = status_.metrics();
  if (metrics.uartOverflow > 0 || metrics.queueOverflow > 0 || metrics.droppedPackets > 0) {
    blink(250) ? statusLed_.setRgb(255, 0, 0) : statusLed_.off();
    return;
  }

  if (now - metrics.lastTxMs < 120) {
    statusLed_.setRgb(0, 64, 255);
    return;
  }

  if (now - metrics.lastRxMs < 120) {
    statusLed_.setRgb(0, 255, 40);
    return;
  }

  if (wifi_.apMode()) {
    blink(500) ? statusLed_.setRgb(0, 64, 255) : statusLed_.off();
    return;
  }

  if (wifi_.state() == WifiState::Connecting) {
    blink(250) ? statusLed_.setRgb(255, 120, 0) : statusLed_.off();
    return;
  }

  if (tcp_.clientCount() > 0) {
    statusLed_.setRgb(0, 180, 180);
    return;
  }

  if (wifi_.state() == WifiState::Connected) {
    statusLed_.setRgb(0, 100, 20);
    return;
  }

  statusLed_.setRgb(8, 8, 8);
}

void FirmwareApp::applyConfig(const DeviceConfig& config) {
  config_ = config;
  rs485_.reconfigure(config_);
  tcp_.stop();
  tcp_.begin(config_, status_);
  tcp_.setTxSink(&FirmwareApp::txSink, this);
}

}  // namespace dm
