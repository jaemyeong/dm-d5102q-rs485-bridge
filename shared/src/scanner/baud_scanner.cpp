#include "baud_scanner.h"

namespace dm {

bool BaudScanner::start(uint32_t startBaud, uint32_t endBaud, uint32_t step, uint16_t sampleMs) {
  if (startBaud < 300 || endBaud > 921600 || endBaud <= startBaud || step == 0) return false;
  running_ = true;
  startBaud_ = startBaud;
  endBaud_ = endBaud;
  step_ = step;
  sampleMs_ = sampleMs < 50 ? 50 : sampleMs;
  cursor_ = startBaud_;
  lastSampleMs_ = 0;
  resultCount_ = 0;
  return true;
}

void BaudScanner::stop() {
  running_ = false;
}

void BaudScanner::poll() {
  if (!running_) return;
  uint32_t now = millis();
  if (lastSampleMs_ != 0 && now - lastSampleMs_ < sampleMs_) return;
  lastSampleMs_ = now;

  if (cursor_ > endBaud_) {
    running_ = false;
    return;
  }
  addSyntheticResult(cursor_);
  cursor_ += step_;
}

bool BaudScanner::running() const {
  return running_;
}

void BaudScanner::writeJson(JsonDocument& doc) const {
  doc["ok"] = true;
  JsonObject data = doc["data"].to<JsonObject>();
  data["running"] = running_;
  data["cursor"] = cursor_;
  data["start"] = startBaud_;
  data["end"] = endBaud_;
  JsonArray rows = data["results"].to<JsonArray>();
  for (size_t i = 0; i < resultCount_; ++i) {
    JsonObject row = rows.add<JsonObject>();
    row["baud"] = results_[i].baud;
    row["score"] = results_[i].score;
    row["bytes"] = results_[i].bytes;
  }
}

const BaudScanResult* BaudScanner::results() const {
  return results_;
}

size_t BaudScanner::resultCount() const {
  return resultCount_;
}

void BaudScanner::addSyntheticResult(uint32_t baud) {
  uint32_t supported[] = {9600, 19200, 38400, 57600, 115200};
  uint32_t nearest = supported[0];
  for (uint32_t value : supported) {
    uint32_t valueDistance = value > baud ? value - baud : baud - value;
    uint32_t nearestDistance = nearest > baud ? nearest - baud : baud - nearest;
    if (valueDistance < nearestDistance) nearest = value;
  }
  uint32_t distance = nearest > baud ? nearest - baud : baud - nearest;
  uint32_t divisor = step_ == 0 ? 1 : step_;
  long rawScore = 80L - static_cast<long>(distance / divisor);
  uint8_t score = distance == 0 ? 96 : static_cast<uint8_t>(rawScore < 0 ? 0 : rawScore);
  BaudScanResult result{baud, score, static_cast<uint16_t>(score * 2)};

  if (resultCount_ < 16) {
    results_[resultCount_++] = result;
  } else {
    size_t worst = 0;
    for (size_t i = 1; i < resultCount_; ++i) {
      if (results_[i].score < results_[worst].score) worst = i;
    }
    if (result.score > results_[worst].score) results_[worst] = result;
  }
}

}  // namespace dm
