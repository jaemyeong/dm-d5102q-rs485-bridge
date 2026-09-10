#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
using String = std::string;
extern uint32_t fakeNow;
constexpr int INPUT = 1, LOW = 0, HIGH = 1;
extern bool fakeButtonPressed;
inline void pinMode(int pin, int mode) { if (pin != 39 || mode != INPUT) __builtin_trap(); }
inline int digitalRead(int pin) { if (pin != 39) __builtin_trap(); return fakeButtonPressed ? LOW : HIGH; }
inline uint32_t millis() { return fakeNow; }
inline uint32_t getCpuFrequencyMhz() { return 240; }
struct FakeSerial {
  std::vector<std::string> messages;
  void begin(unsigned) {}
  void print(const char* value) { messages.emplace_back(value); }
  void println(const char* value) { messages.emplace_back(value); }
  template<typename T> void println(T) { messages.emplace_back("<non-secret diagnostic>"); }
};
extern FakeSerial Serial;
struct FakeEsp {
  unsigned restarts = 0;
  void restart() { ++restarts; }
  uint64_t getEfuseMac() { return 0x112233445566ULL; }
  const char* getChipModel() { return "ESP32-PICO-D4"; }
  uint8_t getChipRevision() { return 1; }
  uint8_t getChipCores() { return 2; }
  uint32_t getFlashChipSize() { return 4194304; }
};
extern FakeEsp ESP;
