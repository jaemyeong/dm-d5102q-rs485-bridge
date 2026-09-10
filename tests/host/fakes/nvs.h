#pragma once
#include <stddef.h>
#include <stdint.h>
using nvs_handle_t = unsigned;
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 1, NVS_READWRITE = 2;
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
esp_err_t nvs_get_blob(nvs_handle_t, const char*, void*, size_t*);
esp_err_t nvs_set_blob(nvs_handle_t, const char*, const void*, size_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_erase_key(nvs_handle_t, const char*);
struct nvs_stats_t { size_t used_entries, free_entries, total_entries, namespace_count; };
inline bool& fakeStatsFail() { static bool value = false; return value; }
inline int nvs_get_stats(const char*, nvs_stats_t* stats) {
  *stats = {60, 444, 504, 2};
  return fakeStatsFail() ? -1 : ESP_OK;
}
