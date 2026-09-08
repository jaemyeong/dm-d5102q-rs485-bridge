#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
inline int mbedtls_sha256_ret(const uint8_t* in, size_t size, uint8_t out[32], int) {
#ifdef __APPLE__
  CC_SHA256(in, static_cast<CC_LONG>(size), out);
#else
  SHA256(in, size, out);
#endif
  return 0;
}
struct mbedtls_sha256_context { std::vector<uint8_t> bytes; };
inline void mbedtls_sha256_init(mbedtls_sha256_context* c) { c->bytes.clear(); }
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context* c, int) { c->bytes.clear(); return 0; }
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context* c, const uint8_t* bytes, size_t size) {
  c->bytes.insert(c->bytes.end(), bytes, bytes + size); return 0;
}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context* c, uint8_t out[32]) { return mbedtls_sha256_ret(c->bytes.data(), c->bytes.size(), out, 0); }
inline void mbedtls_sha256_free(mbedtls_sha256_context* c) { c->bytes.clear(); }
