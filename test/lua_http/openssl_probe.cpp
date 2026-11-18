// Host verification harness, NOT a replacement for the ESP32 TLS HAL.
// Without --fixture it enforces production public IPv4 policy. Fixture mode is test-only.
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include "HttpCore.h"
using namespace ink::http;
struct Tls {
  SSL_CTX* ctx = nullptr;
  SSL* ssl = nullptr;
  int fd = -1;
  bool fixture = false;
  int port = 443;
  const char* ca = nullptr;
  static bool connect(void* p, const char* host, uint32_t timeout, const std::atomic<bool>& cancel) {
    auto& t = *static_cast<Tls*>(p);
    if (cancel.load()) return false;
    t.ctx = SSL_CTX_new(TLS_client_method());
    if (!t.ctx) return false;
    SSL_CTX_set_min_proto_version(t.ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(t.ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(t.ctx, SSL_VERIFY_PEER, nullptr);
    if (t.ca ? SSL_CTX_load_verify_locations(t.ctx, t.ca, nullptr) != 1 : SSL_CTX_set_default_verify_paths(t.ctx) != 1)
      return false;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    char port[8];
    snprintf(port, sizeof(port), "%d", t.port);
    if (getaddrinfo(t.fixture ? "127.0.0.1" : host, port, &hints, &list) != 0) return false;
    bool vetted = list != nullptr;
    for (auto* a = list; a; a = a->ai_next)
      if (!t.fixture && !publicV4(ntohl(reinterpret_cast<sockaddr_in*>(a->ai_addr)->sin_addr.s_addr))) vetted = false;
    if (!vetted) {
      freeaddrinfo(list);
      return false;
    }
    t.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t.fd < 0) {
      perror("socket");
      freeaddrinfo(list);
      return false;
    }
    timeval tv{time_t(timeout / 1000), 0};
    setsockopt(t.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(t.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int rc = ::connect(t.fd, list->ai_addr, list->ai_addrlen);
    freeaddrinfo(list);
    if (rc) return false;
    t.ssl = SSL_new(t.ctx);
    if (!t.ssl) return false;
    SSL_set_fd(t.ssl, t.fd);
    SSL_set_tlsext_host_name(t.ssl, host);
    SSL_set1_host(t.ssl, host);
    return SSL_connect(t.ssl) == 1 && SSL_get_verify_result(t.ssl) == X509_V_OK;
  }
  static int read(void* p, char* b, size_t n) { return SSL_read(static_cast<Tls*>(p)->ssl, b, n); }
  static int write(void* p, const char* b, size_t n) { return SSL_write(static_cast<Tls*>(p)->ssl, b, n); }
  static void close(void* p) {
    auto& t = *static_cast<Tls*>(p);
    if (t.ssl) SSL_free(t.ssl);
    if (t.ctx) SSL_CTX_free(t.ctx);
    if (t.fd >= 0) ::close(t.fd);
  }
  static uint32_t millis(void*) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }
};
int main(int argc, char** argv) {
  // A closed fixture socket must be reported as a transport error, not kill the probe.
  std::signal(SIGPIPE, SIG_IGN);
  if (argc < 2) return 2;
  Request q;
  Result r;
  Scratch scratch;
  Tls t;
  std::atomic<bool> cancel{false};
  if (!parseUrl(argv[1], q.url)) return 2;
  if (argc == 5 && !strcmp(argv[2], "--fixture")) {
    t.fixture = true;
    t.port = atoi(argv[3]);
    t.ca = argv[4];
  }
  Io io{&t, Tls::connect, Tls::read, Tls::write, Tls::close, Tls::millis};
  exchange(q, r, scratch, io, cancel);
  fprintf(stderr, "status=%d error=%s bytes=%zu\n", r.status, r.error ? r.error : "none", r.size);
  if (r.error) return 1;
  fwrite(r.body, 1, r.size, stdout);
  return 0;
}
