#pragma once
#include <nvs.h>
inline int esp_task_wdt_init(unsigned, bool) { return ESP_OK; }
inline int esp_task_wdt_status(void*) { return ESP_OK; }
inline int esp_task_wdt_add(void*) { return ESP_OK; }
inline int esp_task_wdt_reset() { return ESP_OK; }
