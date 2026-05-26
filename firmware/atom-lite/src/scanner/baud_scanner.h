#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace dm {

struct BaudScanResult {
  uint32_t baud = 0;
  uint8_t score = 0;
  uint16_t bytes = 0;
};

class BaudScanner {
 public:
  bool start(uint32_t startBaud, uint32_t endBaud, uint32_t step, uint16_t sampleMs);
  void stop();
  void poll();
  bool running() const;
  void writeJson(JsonDocument& doc) const;
  const BaudScanResult* results() const;
  size_t resultCount() const;

 private:
  void addSyntheticResult(uint32_t baud);

  bool running_ = false;
  uint32_t startBaud_ = 0;
  uint32_t endBaud_ = 0;
  uint32_t step_ = 0;
  uint16_t sampleMs_ = 0;
  uint32_t cursor_ = 0;
  uint32_t lastSampleMs_ = 0;
  BaudScanResult results_[16];
  size_t resultCount_ = 0;
};

}  // namespace dm
