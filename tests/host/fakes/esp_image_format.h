#pragma once
#include <stdint.h>
struct esp_partition_pos_t { uint32_t offset, size; };
struct esp_image_metadata_t { uint32_t image_len; };
constexpr int ESP_IMAGE_VERIFY_SILENT = 1;
inline unsigned& fakeImageVerifies() { static unsigned value = 0; return value; }
inline bool& fakeImageInvalid() { static bool value = false; return value; }
inline int esp_image_verify(int, const esp_partition_pos_t*, esp_image_metadata_t* data) {
  ++fakeImageVerifies(); data->image_len = 1051904; return fakeImageInvalid() ? -1 : 0;
}
