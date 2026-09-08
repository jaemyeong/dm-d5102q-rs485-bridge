#pragma once
#include <stddef.h>
#include <stdint.h>
void esp_fill_random(void*, size_t);
inline uint32_t esp_random() { return 500; }
enum esp_reset_reason_t { ESP_RST_POWERON, ESP_RST_SW, ESP_RST_BROWNOUT, ESP_RST_TASK_WDT };
extern esp_reset_reason_t fakeResetReason;
inline esp_reset_reason_t esp_reset_reason() { return fakeResetReason; }
