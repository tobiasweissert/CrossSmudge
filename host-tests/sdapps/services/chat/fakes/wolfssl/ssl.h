#pragma once
#include <algorithm>
#include <cstring>

#include "PlatformFakes.h"
struct WOLFSSL_CTX {};
struct WOLFSSL {};
inline constexpr int WOLFSSL_SUCCESS = 1, WOLFSSL_TLSV1_2 = 3, WOLFSSL_VERIFY_PEER = 1, WOLFSSL_FILETYPE_PEM = 1,
                     WOLFSSL_SNI_HOST_NAME = 0, WOLFSSL_ERROR_WANT_READ = 2, WOLFSSL_ERROR_WANT_WRITE = 3,
                     WOLFSSL_ERROR_ZERO_RETURN = 6, WOLFSSL_CBIO_ERR_WANT_WRITE = -2, WOLFSSL_CBIO_ERR_CONN_CLOSE = -5,
                     WOLFSSL_CBIO_ERR_WANT_READ = -2;
inline int wolfSSL_Init() { return 1; }
inline void* wolfSSLv23_client_method() { return nullptr; }
inline WOLFSSL_CTX* wolfSSL_CTX_new(void*) {
  static WOLFSSL_CTX c;
  ++fake::contexts;
  return &c;
}
inline void wolfSSL_CTX_free(WOLFSSL_CTX*) { --fake::contexts; }
inline int wolfSSL_CTX_SetMinVersion(WOLFSSL_CTX*, int v) {
  fake::minimum = v == WOLFSSL_TLSV1_2;
  return 1;
}
inline void wolfSSL_CTX_set_verify(WOLFSSL_CTX*, int v, void*) { fake::verify = v == WOLFSSL_VERIFY_PEER; }
inline int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX*, const unsigned char*, long, int) {
  fake::loaded = true;
  return 1;
}
inline void wolfSSL_SetIORecv(WOLFSSL_CTX*, int (*)(WOLFSSL*, char*, int, void*)) {}
inline void wolfSSL_SetIOSend(WOLFSSL_CTX*, int (*)(WOLFSSL*, char*, int, void*)) {}
inline WOLFSSL* wolfSSL_new(WOLFSSL_CTX*) {
  static WOLFSSL s;
  ++fake::sessions;
  return &s;
}
inline void wolfSSL_free(WOLFSSL*) { --fake::sessions; }
inline void wolfSSL_SetIOReadCtx(WOLFSSL*, void*) {}
inline void wolfSSL_SetIOWriteCtx(WOLFSSL*, void*) {}
inline int wolfSSL_check_domain_name(WOLFSSL*, const char* h) {
  fake::identity = h == fake::expectedHost;
  return 1;
}
inline int wolfSSL_UseSNI(WOLFSSL*, int, const char* h, unsigned short) {
  fake::sni = h == fake::expectedHost;
  return 1;
}
inline int wolfSSL_connect(WOLFSSL*) { return fake::chain && fake::host ? 1 : -1; }
inline int wolfSSL_get_error(WOLFSSL*, int n) { return n == 0 ? WOLFSSL_ERROR_ZERO_RETURN : 99; }
inline const char* wolfSSL_get_version(WOLFSSL*) { return "TLSv1.3"; }
inline int wolfSSL_write(WOLFSSL*, const void* p, int n) {
  fake::sent.append(static_cast<const char*>(p), n);
  return n;
}
inline int wolfSSL_read(WOLFSSL*, void* p, int n) {
  size_t count = std::min<size_t>(n, fake::response.size() - fake::readPos);
  memcpy(p, fake::response.data() + fake::readPos, count);
  fake::readPos += count;
  return count;
}
