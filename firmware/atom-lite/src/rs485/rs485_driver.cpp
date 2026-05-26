#include "rs485_driver.h"

#include "board_config.h"

namespace dm {

bool Rs485Driver::begin(HardwareSerial& serial, const DeviceConfig& config, PacketQueue& rxQueue, PacketQueue& txQueue, PacketQueue& txHistory, DeviceStatus& status) {
  serial_ = &serial;
  rxQueue_ = &rxQueue;
  txQueue_ = &txQueue;
  txHistory_ = &txHistory;
  status_ = &status;
  config_ = config;

#if RS485_DE_PIN >= 0
  pinMode(RS485_DE_PIN, OUTPUT);
  setTransmit(false);
#endif
  serial_->setRxBufferSize(config_.uart.rxBufferBytes);
  serial_->begin(config_.uart.baud, serialMode(config_.uart), RS485_RX_PIN, RS485_TX_PIN);
  parser_.configure(framingModeFromString(config_.uart.framing), config_.uart.idleGapMs);
  return true;
}

void Rs485Driver::reconfigure(const DeviceConfig& config) {
  config_ = config;
  if (!serial_) return;
  serial_->end();
  serial_->setRxBufferSize(config_.uart.rxBufferBytes);
  serial_->begin(config_.uart.baud, serialMode(config_.uart), RS485_RX_PIN, RS485_TX_PIN);
  parser_.configure(framingModeFromString(config_.uart.framing), config_.uart.idleGapMs);
}

void Rs485Driver::poll() {
  processRx();
  processTx();
}

bool Rs485Driver::enqueueTx(const uint8_t* data, size_t length) {
  if (!data || length == 0 || length > kMaxPacketBytes || !txQueue_) return false;
  Packet packet;
  packet.direction = PacketDirection::Tx;
  packet.timestampMs = millis();
  packet.length = length;
  memcpy(packet.data, data, length);
  bool ok = txQueue_->push(packet);
  if (!ok && status_) status_->recordQueueOverflow();
  return ok;
}

bool Rs485Driver::busy() const {
  return txBusy_;
}

uint32_t Rs485Driver::serialMode(const UartConfig& config) const {
  bool even = config.parity == "even";
  bool odd = config.parity == "odd";
  if (config.dataBits == 7) {
    if (even && config.stopBits == 2) return SERIAL_7E2;
    if (even) return SERIAL_7E1;
    if (odd && config.stopBits == 2) return SERIAL_7O2;
    if (odd) return SERIAL_7O1;
    return config.stopBits == 2 ? SERIAL_7N2 : SERIAL_7N1;
  }
  if (even && config.stopBits == 2) return SERIAL_8E2;
  if (even) return SERIAL_8E1;
  if (odd && config.stopBits == 2) return SERIAL_8O2;
  if (odd) return SERIAL_8O1;
  return config.stopBits == 2 ? SERIAL_8N2 : SERIAL_8N1;
}

void Rs485Driver::processRx() {
  if (!serial_ || !rxQueue_) return;
  uint32_t now = millis();
  while (serial_->available() > 0) {
    int value = serial_->read();
    if (value < 0) break;
    if (status_) status_->recordRawByte();
    Packet packet;
    if (parser_.pushByte(static_cast<uint8_t>(value), now, packet)) {
      if (!rxQueue_->push(packet)) {
        if (status_) status_->recordQueueOverflow();
      } else if (status_) {
        status_->recordRx(packet.length);
      }
    }
  }
  Packet packet;
  if (parser_.flushIdle(now, packet)) {
    if (!rxQueue_->push(packet)) {
      if (status_) status_->recordQueueOverflow();
    } else if (status_) {
      status_->recordRx(packet.length);
    }
  }
}

void Rs485Driver::processTx() {
  if (!serial_ || !txQueue_) return;
  Packet packet;
  if (!txQueue_->pop(packet)) return;

  txBusy_ = true;
  setTransmit(true);
  serial_->write(packet.data, packet.length);
  serial_->flush();
  setTransmit(false);
  txBusy_ = false;

  packet.timestampMs = millis();
  if (txHistory_) txHistory_->push(packet);
  if (status_) status_->recordTx(packet.length);
}

void Rs485Driver::setTransmit(bool enabled) {
#if RS485_DE_PIN >= 0
  digitalWrite(RS485_DE_PIN, enabled ? HIGH : LOW);
#else
  (void)enabled;
#endif
}

}  // namespace dm
