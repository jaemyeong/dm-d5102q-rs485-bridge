#include "packet_queue.h"

namespace dm {

PacketQueue::PacketQueue(size_t capacity) : capacity_(capacity) {
  packets_ = new Packet[capacity_];
}

PacketQueue::~PacketQueue() {
  delete[] packets_;
}

bool PacketQueue::push(const Packet& packet) {
  if (!packets_ || capacity_ == 0) return false;
  if (size_ == capacity_) {
    dropped_++;
    return false;
  }
  packets_[tail_] = packet;
  tail_ = (tail_ + 1) % capacity_;
  size_++;
  return true;
}

bool PacketQueue::pop(Packet& packet) {
  if (size_ == 0 || !packets_) return false;
  packet = packets_[head_];
  head_ = (head_ + 1) % capacity_;
  size_--;
  return true;
}

bool PacketQueue::peek(size_t index, Packet& packet) const {
  if (index >= size_ || !packets_) return false;
  packet = packets_[(head_ + index) % capacity_];
  return true;
}

void PacketQueue::clear() {
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

size_t PacketQueue::size() const {
  return size_;
}

size_t PacketQueue::capacity() const {
  return capacity_;
}

size_t PacketQueue::dropped() const {
  return dropped_;
}

}  // namespace dm
