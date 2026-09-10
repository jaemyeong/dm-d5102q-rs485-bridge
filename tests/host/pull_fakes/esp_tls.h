#pragma once
#include "../fakes/esp_ota_ops.h"
#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <assert.h>
#include <functional>
constexpr int ESP_TLS_ERR_SSL_WANT_READ = -100, ESP_TLS_ERR_SSL_WANT_WRITE = -101;
constexpr int ESP_ERR_NO_MEM = 0x101;
struct esp_tls_cfg_t {
  int timeout_ms = 0;
  bool non_block = false, skip_common_name = false, is_plain_tcp = false;
  const char* common_name = nullptr;
  esp_err_t (*crt_bundle_attach)(void*) = nullptr;
};
struct FakeTlsError {
  int error, code, flags;
  FakeTlsError(int error = 0, int code = 0, int flags = 0) : error(error), code(code), flags(flags) {}
};
struct esp_tls_t {
  std::string response; size_t offset = 0;
  FakeTlsError error;
  FakeTlsError* error_handle = &error;
};
struct TlsFake {
  std::deque<std::string> responses;
  std::vector<std::string> requests, hosts;
  std::vector<int> timeouts;
  bool failConnect = false, timeoutConnect = false, failInit = false, failWrite = false, failRead = false;
  uint32_t connectDelayMs = 13;
  FakeTlsError error{0x8007, -9984, 8};
  unsigned allocated = 0, destroyed = 0, errorsRead = 0;
  size_t readFragment = 17, readCalls = 0, readBytes = 0;
  unsigned wantReads = 0;
  int wantCode = ESP_TLS_ERR_SSL_WANT_READ;
  std::function<void()> afterRead;
};
extern TlsFake tlsFake;
extern uint32_t fakeNow;
inline esp_tls_t* esp_tls_init() {
  if (tlsFake.failInit) return nullptr;
  ++tlsFake.allocated; return new esp_tls_t;
}
inline int esp_tls_conn_new_sync(const char* host, int, int port, const esp_tls_cfg_t* config, esp_tls_t* tls) {
  assert(port == 443 && config && config->crt_bundle_attach && config->timeout_ms > 0 && config->timeout_ms <= 15000 && config->non_block);
  assert(!config->skip_common_name && !config->is_plain_tcp && !config->common_name);
  tlsFake.hosts.push_back(host); tlsFake.requests.emplace_back();
  tlsFake.timeouts.push_back(config->timeout_ms);
  fakeNow += tlsFake.connectDelayMs;
  if (tlsFake.failConnect || tlsFake.timeoutConnect || tlsFake.responses.empty()) {
    tls->error = tlsFake.error; return tlsFake.timeoutConnect ? 0 : -1;
  }
  tls->response = tlsFake.responses.front(); tlsFake.responses.pop_front(); return 1;
}
inline int esp_tls_conn_write(esp_tls_t* tls, const void* bytes, size_t size) {
  if (tlsFake.failWrite) { tls->error = tlsFake.error; return tlsFake.error.code; }
  tlsFake.requests.back().append(static_cast<const char*>(bytes), size); return int(size);
}
inline int esp_tls_conn_read(esp_tls_t* tls, void* bytes, size_t size) {
  ++tlsFake.readCalls;
  if (tlsFake.wantReads) {
    --tlsFake.wantReads;
    if (tlsFake.afterRead) tlsFake.afterRead();
    return tlsFake.wantCode;
  }
  if (tlsFake.failRead) { tls->error = tlsFake.error; return tlsFake.error.code; }
  size = std::min({size, tlsFake.readFragment, tls->response.size() - tls->offset});
  tlsFake.readBytes += size;
  memcpy(bytes, tls->response.data() + tls->offset, size); tls->offset += size;
  if (tlsFake.afterRead) tlsFake.afterRead();
  return int(size);
}
inline esp_err_t esp_tls_get_and_clear_last_error(FakeTlsError* handle, int* code, int* flags) {
  assert(handle); ++tlsFake.errorsRead;
  *code = handle->code; *flags = handle->flags;
  const int error = handle->error; *handle = FakeTlsError{}; return error;
}
inline int esp_tls_conn_destroy(esp_tls_t* tls) { ++tlsFake.destroyed; delete tls; return 0; }
