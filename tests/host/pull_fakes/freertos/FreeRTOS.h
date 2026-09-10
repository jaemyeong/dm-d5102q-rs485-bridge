#pragma once
#include <stdint.h>
#include <deque>
#include <vector>
#include <functional>
#include <string.h>
extern uint32_t fakeNow;
extern uint32_t queueReceiveAdvance;
using BaseType_t = int;
using StackType_t = uint32_t;
using TaskHandle_t = void*;
struct StaticTask_t {};
struct StaticQueue_t { size_t count = 0, item = 0; std::deque<std::vector<uint8_t>> data; };
using QueueHandle_t = StaticQueue_t*;
constexpr int pdTRUE = 1;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
inline uint32_t pdMS_TO_TICKS(uint32_t value) { return value; }
inline void vTaskDelay(uint32_t ticks) { fakeNow += ticks; }
inline uint32_t uxTaskGetStackHighWaterMark(void*) { return 4096; } // Synthetic, not measured.
inline TaskHandle_t xTaskCreateStaticPinnedToCore(void (*)(void*), const char*, uint32_t, void*, int, StackType_t*, StaticTask_t* storage, int) {
  return storage; // No real thread. Harness drives a single worker job deterministically.
}
extern std::function<void(QueueHandle_t)> queueSent;
inline QueueHandle_t xQueueCreateStatic(size_t count, size_t item, uint8_t*, StaticQueue_t* storage) {
  storage->count = count; storage->item = item; storage->data.clear(); return storage;
}
inline int xQueueSend(QueueHandle_t queue, const void* value, uint32_t) {
  if (queue->data.size() >= queue->count) return 0;
  const auto* bytes = static_cast<const uint8_t*>(value);
  queue->data.emplace_back(bytes, bytes + queue->item);
  if (queueSent) queueSent(queue);
  return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void* value, uint32_t ticks) {
  if (queue->data.empty()) { fakeNow += ticks; return 0; }
  memcpy(value, queue->data.front().data(), queue->item); queue->data.pop_front();
  fakeNow += queueReceiveAdvance;
  return pdTRUE;
}
