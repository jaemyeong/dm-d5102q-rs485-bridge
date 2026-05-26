#include "packet_parser.h"

namespace dm {

void PacketParser::configure(FramingMode mode, uint16_t idleGapMs, uint8_t delimiter, size_t lengthFieldOffset) {
  mode_ = mode;
  idleGapMs_ = idleGapMs == 0 ? 20 : idleGapMs;
  delimiter_ = delimiter;
  lengthFieldOffset_ = lengthFieldOffset;
  reset();
}

bool PacketParser::pushByte(uint8_t byte, uint32_t nowMs, Packet& completed) {
  if (mode_ == FramingMode::InterByteTimeout && length_ > 0 && nowMs - lastByteMs_ > idleGapMs_) {
    bool emitted = emit(completed);
    append(byte);
    lastByteMs_ = nowMs;
    return emitted;
  }

  append(byte);
  lastByteMs_ = nowMs;

  if (mode_ == FramingMode::Delimiter && byte == delimiter_) {
    return emit(completed);
  }
  if (mode_ == FramingMode::LengthBased && shouldEmitLengthBased()) {
    return emit(completed);
  }
  return false;
}

bool PacketParser::flushIdle(uint32_t nowMs, Packet& completed) {
  if (length_ == 0) return false;
  if (mode_ == FramingMode::InterByteTimeout && nowMs - lastByteMs_ >= idleGapMs_) {
    return emit(completed);
  }
  return false;
}

void PacketParser::reset() {
  length_ = 0;
  lastByteMs_ = 0;
}

size_t PacketParser::buffered() const {
  return length_;
}

bool PacketParser::emit(Packet& completed) {
  if (length_ == 0) return false;
  completed.direction = PacketDirection::Rx;
  completed.timestampMs = millis();
  completed.length = length_;
  memcpy(completed.data, buffer_, length_);
  completed.message = "";
  reset();
  return true;
}

bool PacketParser::append(uint8_t byte) {
  if (length_ >= kMaxPacketBytes) {
    reset();
    return false;
  }
  buffer_[length_++] = byte;
  return true;
}

bool PacketParser::shouldEmitLengthBased() const {
  if (length_ <= lengthFieldOffset_) return false;
  size_t expectedPayload = buffer_[lengthFieldOffset_];
  size_t expectedTotal = lengthFieldOffset_ + 1 + expectedPayload;
  return expectedPayload > 0 && length_ >= expectedTotal;
}

FramingMode framingModeFromString(const String& value) {
  if (value == "delimiter") return FramingMode::Delimiter;
  if (value == "length" || value == "length-based") return FramingMode::LengthBased;
  return FramingMode::InterByteTimeout;
}

String framingModeToString(FramingMode mode) {
  switch (mode) {
    case FramingMode::Delimiter: return "delimiter";
    case FramingMode::LengthBased: return "length";
    case FramingMode::InterByteTimeout: return "inter-byte";
  }
  return "inter-byte";
}

}  // namespace dm
