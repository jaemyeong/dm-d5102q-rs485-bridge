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
  if (config_.log.enabled) log_.begin(config_.log.port, config_.log.maxClients);
  emitBootLog();
  scheduler_.every(1000, &FirmwareApp::tickStatusTask, this);
  scheduler_.every(config_.log.heartbeatMs, &FirmwareApp::heartbeatTask, this);
}

void FirmwareApp::loop() {
  checkFactoryResetButton();
  wifi_.poll();
  rs485_.poll();
  tcp_.poll();
  scanner_.poll();
  web_.poll();
  log_.poll();
  processPackets();
  emitStateTransitions();
  updateStatusLed();
  scheduler_.poll();
  if (pendingResetAtMs_ != 0 && millis() >= pendingResetAtMs_) {
    pendingResetAtMs_ = 0;
    if (pendingResetFullWipe_) {
      log_.log("[factory-reset] executing eraseAll now");
      delay(20);
      configStore_.eraseAll();
    } else {
      log_.log("[factory-reset] executing resetExceptWifi now");
      delay(20);
      configStore_.resetExceptWifi();
    }
    delay(150);
    ESP.restart();
  }
  web_.pollRebootDeadline();
}

bool FirmwareApp::queueTx(const uint8_t* data, size_t length) {
  return rs485_.enqueueTx(data, length);
}

void FirmwareApp::factoryResetSettings() {
  log_.log("[factory-reset] settings-only scheduled (wifi kept), wipe in 600ms");
  pendingResetFullWipe_ = false;
  pendingResetAtMs_ = millis() + 600;
}

void FirmwareApp::factoryResetAll() {
  log_.log("[factory-reset] full NVS wipe scheduled, in 600ms");
  pendingResetFullWipe_ = true;
  pendingResetAtMs_ = millis() + 600;
}

bool FirmwareApp::txSink(const uint8_t* data, size_t length, void* ctx) {
  return static_cast<FirmwareApp*>(ctx)->queueTx(data, length);
}

bool FirmwareApp::saveConfigSink(const DeviceConfig& config, void* ctx) {
  static_cast<FirmwareApp*>(ctx)->applyConfig(config);
  return true;
}

void FirmwareApp::factoryResetSink(void* ctx, bool fullWipe) {
  if (fullWipe) {
    static_cast<FirmwareApp*>(ctx)->factoryResetAll();
  } else {
    static_cast<FirmwareApp*>(ctx)->factoryResetSettings();
  }
}

void FirmwareApp::tickStatusTask(void* ctx) {
  static_cast<FirmwareApp*>(ctx)->tickStatus();
}

void FirmwareApp::checkFactoryResetButton() {
  bool pressed = digitalRead(FACTORY_RESET_BTN_PIN) == LOW;
  if (!pressed) {
    if (resetHoldStartMs_ != 0 && !resetTriggered_) {
      uint32_t held = millis() - resetHoldStartMs_;
      if (held >= 8000) {
        resetTriggered_ = true;
        factoryResetAll();
      } else if (held >= 5000) {
        resetTriggered_ = true;
        factoryResetSettings();
      }
      // held < 5000: cancel, no action
    }
    resetHoldStartMs_ = 0;
    return;
  }
  if (resetHoldStartMs_ == 0) resetHoldStartMs_ = millis();
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

  if (resetHoldStartMs_ != 0 && !resetTriggered_) {
    uint32_t held = now - resetHoldStartMs_;
    if (held >= 8000) {
      statusLed_.setRgb(255, 0, 0);
    } else if (held >= 5000) {
      statusLed_.setRgb(255, 200, 0);
    } else {
      blink(200) ? statusLed_.setRgb(255, 120, 0) : statusLed_.off();
    }
    return;
  }

  if (pendingResetAtMs_ != 0) {
    blink(80) ? statusLed_.setRgb(255, 0, 0) : statusLed_.off();
    return;
  }

  if (web_.ota().updating() || web_.ota().rebootPending()) {
    blink(180) ? statusLed_.setRgb(140, 0, 255) : statusLed_.off();
    return;
  }

  const RuntimeMetrics metrics = status_.metrics();
  if (metrics.uartOverflow > 0 || metrics.queueOverflow > 0) {
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
  log_.stop();
  if (config_.log.enabled) log_.begin(config_.log.port, config_.log.maxClients);
  log_.log("[config] applied baud=%u tcpPort=%u logPort=%u logEnabled=%d",
           config_.uart.baud, config_.tcp.port, config_.log.port,
           config_.log.enabled ? 1 : 0);
}

void FirmwareApp::emitBootLog() {
  log_.log("[boot] dm-d5102q-bridge v0.1.9 build=%s %s", __DATE__, __TIME__);
  log_.log("[boot] device=\"%s\" board=%s", config_.deviceName.c_str(), ARDUINO_BOARD);
  const char parity = config_.uart.parity == "even" ? 'E'
                    : config_.uart.parity == "odd"  ? 'O'
                    : 'N';
  log_.log("[boot] uart RX=GPIO%d TX=GPIO%d baud=%u %u%c%u framing=%s idleGapMs=%u rxBuf=%u avail=%d",
           RS485_RX_PIN, RS485_TX_PIN, config_.uart.baud,
           config_.uart.dataBits, parity, config_.uart.stopBits,
           config_.uart.framing.c_str(), config_.uart.idleGapMs,
           static_cast<unsigned>(config_.uart.rxBufferBytes),
           Serial2.available());
  log_.log("[boot] tcp mode=%s host=%s port=%u maxClients=%u",
           config_.tcp.mode.c_str(), config_.tcp.host.c_str(),
           config_.tcp.port, config_.tcp.maxClients);
  log_.log("[boot] log enabled=%d port=%u maxClients=%u heartbeatMs=%u",
           config_.log.enabled ? 1 : 0, config_.log.port,
           config_.log.maxClients, config_.log.heartbeatMs);
  log_.log("[boot] wifi ssid=\"%s\" forceAp=%d ip=%s rssi=%d",
           config_.wifi.ssid.c_str(), resetTriggered_ ? 1 : 0,
           wifi_.ipAddress().c_str(), static_cast<int>(wifi_.rssi()));
}

void FirmwareApp::emitStateTransitions() {
  WifiState s = wifi_.state();
  if (s != lastWifiState_) {
    lastWifiState_ = s;
    const char* name = "?";
    switch (s) {
      case WifiState::Idle: name = "idle"; break;
      case WifiState::Connecting: name = "connecting"; break;
      case WifiState::Connected: name = "connected"; break;
      case WifiState::AccessPoint: name = "ap"; break;
    }
    log_.log("[wifi] state=%s ip=%s rssi=%d", name,
             wifi_.ipAddress().c_str(), static_cast<int>(wifi_.rssi()));
  }
  uint8_t tc = tcp_.clientCount();
  if (tc != lastTcpClients_) {
    log_.log("[tcp] clients %u -> %u", lastTcpClients_, tc);
    lastTcpClients_ = tc;
  }
  bool ou = web_.ota().updating();
  if (ou != lastOtaUpdating_) {
    lastOtaUpdating_ = ou;
    log_.log("[ota] updating=%d", ou ? 1 : 0);
  }
  bool rp = web_.ota().rebootPending();
  if (rp != lastRebootPending_) {
    lastRebootPending_ = rp;
    log_.log("[ota] rebootPending=%d", rp ? 1 : 0);
  }
}

void FirmwareApp::heartbeatTask(void* ctx) {
  auto self = static_cast<FirmwareApp*>(ctx);
  if (!self->log_.started()) return;
  RuntimeMetrics m = self->status_.metrics();
  uint32_t now = millis();
  uint32_t lastRxAgo = (m.lastRxMs == 0) ? 0xFFFFFFFFu : (now - m.lastRxMs);
  uint32_t lastTxAgo = (m.lastTxMs == 0) ? 0xFFFFFFFFu : (now - m.lastTxMs);
  const char* wifiName = "?";
  switch (self->wifi_.state()) {
    case WifiState::Idle: wifiName = "idle"; break;
    case WifiState::Connecting: wifiName = "connecting"; break;
    case WifiState::Connected: wifiName = "connected"; break;
    case WifiState::AccessPoint: wifiName = "ap"; break;
  }
  uint32_t lastRawAgo = (m.lastRawByteMs == 0) ? 0xFFFFFFFFu : (now - m.lastRawByteMs);
  int s2avail = Serial2.available();
  self->log_.log(
    "[hb] up=%lus wifi=%s ip=%s rssi=%d tcp=%u log=%u rawB=%u rxB=%u txB=%u "
    "rxP=%u txP=%u s2avail=%d qUse=%u qOf=%u uOf=%u drop=%u rej=%u "
    "lastRawAgoMs=%lu lastRxAgoMs=%lu lastTxAgoMs=%lu heap=%u",
    static_cast<unsigned long>((now - m.bootMs) / 1000UL),
    wifiName, self->wifi_.ipAddress().c_str(), static_cast<int>(m.rssi),
    static_cast<unsigned>(m.tcpClients), static_cast<unsigned>(self->log_.clientCount()),
    static_cast<unsigned>(m.rawBytes),
    static_cast<unsigned>(m.rxBytes), static_cast<unsigned>(m.txBytes),
    static_cast<unsigned>(m.rxPackets), static_cast<unsigned>(m.txPackets),
    s2avail,
    static_cast<unsigned>(m.queueUsage), static_cast<unsigned>(m.queueOverflow),
    static_cast<unsigned>(m.uartOverflow), static_cast<unsigned>(m.droppedPackets),
    static_cast<unsigned>(m.tcpRejected),
    static_cast<unsigned long>(lastRawAgo),
    static_cast<unsigned long>(lastRxAgo), static_cast<unsigned long>(lastTxAgo),
    static_cast<unsigned>(ESP.getFreeHeap()));
}

}  // namespace dm
