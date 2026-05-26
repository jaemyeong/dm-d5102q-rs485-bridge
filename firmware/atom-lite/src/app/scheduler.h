#pragma once

#include <Arduino.h>

namespace dm {

using ScheduledCallback = void (*)(void* ctx);

class Scheduler {
 public:
  bool every(uint32_t intervalMs, ScheduledCallback callback, void* ctx);
  void poll();
  void clear();

 private:
  struct Task {
    uint32_t intervalMs = 0;
    uint32_t lastRunMs = 0;
    ScheduledCallback callback = nullptr;
    void* ctx = nullptr;
    bool active = false;
  };

  Task tasks_[8];
};

}  // namespace dm
