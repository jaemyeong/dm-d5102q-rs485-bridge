#pragma once
#include <nvs.h>
#include <stdint.h>
#include <vector>
constexpr int ESP_FAIL = -1, ESP_ERR_NOT_FOUND = 257, ESP_ERR_INVALID_STATE = 259;
struct esp_partition_t { uint32_t address, size; };
using esp_ota_handle_t = unsigned;
enum esp_ota_img_states_t { ESP_OTA_IMG_NEW, ESP_OTA_IMG_PENDING_VERIFY, ESP_OTA_IMG_VALID };
struct FakeOta {
  unsigned begins = 0, writes = 0, ends = 0, selects = 0, aborts = 0, confirms = 0, rollbacks = 0;
  int beginError = 0, writeError = 0, endError = 0, selectError = 0, confirmError = 0;
  esp_ota_img_states_t state = ESP_OTA_IMG_VALID;
  std::vector<uint8_t> image;
};
inline FakeOta& fakeOta() { static FakeOta value; return value; }
inline const esp_partition_t* esp_ota_get_running_partition() { static esp_partition_t p{0x10000, 0x180000}; return &p; }
inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) { static esp_partition_t p{0x190000, 0x180000}; return &p; }
inline int esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t* state) { *state = fakeOta().state; return ESP_OK; }
inline int esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t* out) { ++fakeOta().begins; *out = 1; fakeOta().image.clear(); return fakeOta().beginError; }
inline int esp_ota_write(esp_ota_handle_t, const void* bytes, size_t size) {
  ++fakeOta().writes; const auto* data = static_cast<const uint8_t*>(bytes);
  fakeOta().image.insert(fakeOta().image.end(), data, data + size); return fakeOta().writeError;
}
inline int esp_ota_end(esp_ota_handle_t) { ++fakeOta().ends; return fakeOta().endError; }
inline int esp_ota_set_boot_partition(const esp_partition_t*) { ++fakeOta().selects; return fakeOta().selectError; }
inline int esp_ota_abort(esp_ota_handle_t) { ++fakeOta().aborts; return ESP_OK; }
inline int esp_ota_mark_app_valid_cancel_rollback() {
  ++fakeOta().confirms; if (!fakeOta().confirmError) fakeOta().state = ESP_OTA_IMG_VALID;
  return fakeOta().confirmError;
}
inline int esp_ota_mark_app_invalid_rollback_and_reboot() { ++fakeOta().rollbacks; return ESP_FAIL; }
