#pragma once

#include <Arduino.h>
#include "packet.h"

namespace dm {

class PacketQueue {
 public:
  explicit PacketQueue(size_t capacity = 32);
  ~PacketQueue();

  bool push(const Packet& packet);
  bool pop(Packet& packet);
  bool peek(size_t index, Packet& packet) const;
  void clear();

  size_t size() const;
  size_t capacity() const;
  size_t dropped() const;

 private:
  Packet* packets_ = nullptr;
  size_t capacity_ = 0;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
  size_t dropped_ = 0;
};

}  // namespace dm
