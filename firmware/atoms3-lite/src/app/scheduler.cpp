#include "scheduler.h"

namespace dm {

bool Scheduler::every(uint32_t intervalMs, ScheduledCallback callback, void* ctx) {
  for (Task& task : tasks_) {
    if (task.active) continue;
    task.intervalMs = intervalMs;
    task.lastRunMs = millis();
    task.callback = callback;
    task.ctx = ctx;
    task.active = true;
    return true;
  }
  return false;
}

void Scheduler::poll() {
  uint32_t now = millis();
  for (Task& task : tasks_) {
    if (!task.active || !task.callback) continue;
    if (now - task.lastRunMs >= task.intervalMs) {
      task.lastRunMs = now;
      task.callback(task.ctx);
    }
  }
}

void Scheduler::clear() {
  for (Task& task : tasks_) task = Task();
}

}  // namespace dm
