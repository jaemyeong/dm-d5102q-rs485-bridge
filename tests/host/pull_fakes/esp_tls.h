#pragma once
#include "../fakes/esp_ota_ops.h"
#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <assert.h>
constexpr int ESP_TLS_ERR_SSL_WANT_READ = -100, ESP_TLS_ERR_SSL_WANT_WRITE = -101;
struct esp_tls_cfg_t {
  int timeout_ms = 0;
  bool non_block = false, skip_common_name = false, is_plain_tcp = false;
  const char* common_name = nullptr;
  esp_err_t (*crt_bundle_attach)(void*) = nullptr;
};
struct esp_tls_t { std::string response; size_t offset = 0; };
struct TlsFake {
  std::deque<std::string> responses;
  std::vector<std::string> requests, hosts;
  bool failConnect = false;
  unsigned allocated = 0, destroyed = 0;
};
extern TlsFake tlsFake;
inline esp_tls_t* esp_tls_init() { ++tlsFake.allocated; return new esp_tls_t; }
inline int esp_tls_conn_new_sync(const char* host, int, int port, const esp_tls_cfg_t* config, esp_tls_t* tls) {
  assert(port == 443 && config && config->crt_bundle_attach && config->timeout_ms == 3000 && config->non_block);
  assert(!config->skip_common_name && !config->is_plain_tcp && !config->common_name);
  tlsFake.hosts.push_back(host); tlsFake.requests.emplace_back();
  if (tlsFake.failConnect || tlsFake.responses.empty()) return -1;
  tls->response = tlsFake.responses.front(); tlsFake.responses.pop_front(); return 1;
}
inline int esp_tls_conn_write(esp_tls_t*, const void* bytes, size_t size) {
  tlsFake.requests.back().append(static_cast<const char*>(bytes), size); return int(size);
}
inline int esp_tls_conn_read(esp_tls_t* tls, void* bytes, size_t size) {
  size = std::min({size, size_t(17), tls->response.size() - tls->offset}); // Fragment TLS records.
  memcpy(bytes, tls->response.data() + tls->offset, size); tls->offset += size; return int(size);
}
inline int esp_tls_conn_destroy(esp_tls_t* tls) { ++tlsFake.destroyed; delete tls; return 0; }
