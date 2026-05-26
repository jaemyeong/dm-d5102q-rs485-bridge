#pragma once

#include <Arduino.h>
#include "packet.h"

namespace dm {

enum class FramingMode : uint8_t {
  InterByteTimeout,
  Delimiter,
  LengthBased
};

class PacketParser {
 public:
  void configure(FramingMode mode, uint16_t idleGapMs, uint8_t delimiter = '\n', size_t lengthFieldOffset = 2);
  bool pushByte(uint8_t byte, uint32_t nowMs, Packet& completed);
  bool flushIdle(uint32_t nowMs, Packet& completed);
  void reset();
  size_t buffered() const;

 private:
  bool emit(Packet& completed);
  bool append(uint8_t byte);
  bool shouldEmitLengthBased() const;

  FramingMode mode_ = FramingMode::InterByteTimeout;
  uint16_t idleGapMs_ = 20;
  uint8_t delimiter_ = '\n';
  size_t lengthFieldOffset_ = 2;
  uint8_t buffer_[kMaxPacketBytes] = {0};
  size_t length_ = 0;
  uint32_t lastByteMs_ = 0;
};

FramingMode framingModeFromString(const String& value);
String framingModeToString(FramingMode mode);

}  // namespace dm
