#pragma once

#include <Arduino.h>
#include "../utils/hex.h"

namespace dm {

enum class PacketDirection : uint8_t {
  Rx,
  Tx,
  Error,
  System
};

struct Packet {
  PacketDirection direction = PacketDirection::System;
  uint32_t timestampMs = 0;
  uint8_t data[kMaxPacketBytes] = {0};
  size_t length = 0;
  String message;

  String hex() const {
    return bytesToHex(data, length);
  }
};

inline const char* packetDirectionName(PacketDirection direction) {
  switch (direction) {
    case PacketDirection::Rx: return "rx";
    case PacketDirection::Tx: return "tx";
    case PacketDirection::Error: return "error";
    case PacketDirection::System: return "system";
  }
  return "system";
}

}  // namespace dm
