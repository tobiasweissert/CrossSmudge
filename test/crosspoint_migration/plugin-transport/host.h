#define CROSSINK_GIT_SHA "0123456789abcdef0123456789abcdef01234567"
#define CROSSINK_GIT_DIRTY "1"
#include <arpa/inet.h>
#include <netdb.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
inline std::vector<std::string> diagnostics;
template <typename... Args>
inline void captureLog(const char* tag, const char* format, Args... args) {
  char message[96];
  const int n = snprintf(message, sizeof(message), format, args...);
  assert(std::string(tag) == "PHTP" && n >= 0 && n < static_cast<int>(sizeof(message)));
  diagnostics.emplace_back(message);
}
#define LOG_ERR(...) captureLog(__VA_ARGS__)
namespace freeink {
class SecureHttpClient {};
}  // namespace freeink
class String {
 public:
  std::string s;
  String() = default;
  String(const char* p) : s(p) {}
  void remove(size_t) { s.clear(); }
  size_t length() const { return s.size(); }
  bool concat(const char* p, size_t n) {
    s.append(p, n);
    return true;
  }
};
struct WifiPowerSaveGuard {
  ~WifiPowerSaveGuard() {}
};
struct Clock {
  bool valid = true, syncOk = false;
  unsigned syncCalls = 0;
  std::function<void()> duringSync;
  bool hasSynchronizedSystemTime() { return valid; }
  bool syncSystemTimeFromNTP() {
    ++syncCalls;
    if (duringSync) duringSync();
    valid = syncOk;
    return syncOk;
  }
};
inline Clock halClock;
struct File {
  bool syncOk = true, closeOk = true, writeOk = true, opened = false;
  std::string data;
};
inline File fs;
struct HalFile {
  size_t write(const uint8_t* p, size_t n) {
    if (!fs.writeOk) return 0;
    fs.data.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
  bool sync() { return fs.syncOk; }
  bool close() {
    fs.opened = false;
    return fs.closeOk;
  }
};
struct Store {
  std::string manifestFixture;
  bool exists(const char*) { return false; }
  bool ensureDirectoryExists(const char*) { return true; }
  bool writeFile(const char*, const String&) { return true; }
  bool readFileToString(const char*, const std::string&, size_t cap, std::string& out) {
    out = manifestFixture;
    return out.size() <= cap;
  }
  bool removed = false;
  bool openFileForWrite(const char*, const char*, HalFile&) {
    fs.opened = true;
    fs.data.clear();
    return true;
  }
  bool remove(const char*) {
    removed = true;
    return true;
  }
};
inline Store Storage;
inline bool allowedFile(const std::string& p) { return p == "/book.epub"; }
namespace pluginhttp {
using Headers = std::vector<std::pair<std::string, std::string>>;
using StreamStart = std::function<bool(int, const Headers&, long)>;
using StreamData = std::function<bool(const uint8_t*, size_t)>;
std::string escapeUrl(const std::string&);
int requestStream(freeink::SecureHttpClient*, const std::string&, const std::string&, const std::string&,
                  const Headers&, const StreamStart&, const StreamData&, bool&, const std::function<bool()>&, size_t);
}  // namespace pluginhttp
inline time_t clockSeconds = 1795080000;  // synthetic November 2026 fixture, not a live clock claim
inline time_t fake_time(time_t* out) {
  if (out) *out = clockSeconds;
  return clockSeconds;
}
#define time fake_time
inline uint32_t tick = 0, advance = 0;
inline uint32_t millis() {
  auto n = tick;
  tick += advance;
  return n;
}
inline void delay(int n) { tick += n; }
inline int64_t esp_timer_get_time() { return static_cast<int64_t>(tick) * 1000; }
struct mbedtls_x509_time {
  int year = 2020, mon = 1, day = 1, hour = 0, min = 0, sec = 0;
};
struct Raw {
  size_t len = 10;
};
struct mbedtls_x509_crt {
  Raw raw;
  mbedtls_x509_time valid_from, valid_to{2099, 1, 1, 0, 0, 0};
  mbedtls_x509_crt* next = nullptr;
};
inline mbedtls_x509_crt certificate;
struct mbedtls_ssl_context {};
inline mbedtls_ssl_context ssl;
inline unsigned verifyFlags = 0;
inline unsigned mbedtls_ssl_get_verify_result(mbedtls_ssl_context*) { return verifyFlags; }
inline const mbedtls_x509_crt* mbedtls_ssl_get_peer_cert(mbedtls_ssl_context*) { return &certificate; }
inline int esp_crt_bundle_attach(void*) { return 0; }
constexpr int ESP_TLS_VER_TLS_1_2 = 2, ESP_TLS_ERR_SSL_WANT_READ = -2, ESP_TLS_ERR_SSL_WANT_WRITE = -3;
struct esp_tls_cfg_t {
  int (*crt_bundle_attach)(void*) = nullptr;
  const char* common_name = nullptr;
  bool skip_common_name = true;
  int tls_version = 0, timeout_ms = 0;
  bool non_block = false;
};
struct esp_tls_t {
  size_t pos = 0;
  std::string reply;
  size_t index;
};
inline std::vector<std::string> replies, writes, targets, hosts;
inline size_t created = 0, destroyed = 0, dnsCalls = 0;
inline int handshake = 1;
inline std::vector<std::string> dnsAnswers{"8.8.8.8"};
inline esp_tls_t* esp_tls_init() {
  auto* t = new esp_tls_t;
  t->index = created++;
  t->reply = t->index < replies.size() ? replies[t->index] : "";
  writes.emplace_back();
  return t;
}
inline int esp_tls_conn_destroy(esp_tls_t* t) {
  ++destroyed;
  delete t;
  return 0;
}
inline int esp_tls_conn_new_async(const char* ip, size_t, uint16_t, const esp_tls_cfg_t* cfg, esp_tls_t*) {
  assert(cfg->crt_bundle_attach == esp_crt_bundle_attach);
  assert(!cfg->skip_common_name);
  assert(cfg->tls_version == ESP_TLS_VER_TLS_1_2);
  assert(cfg->non_block);
  if (handshake != 0) {
    targets.emplace_back(ip);
    hosts.emplace_back(cfg->common_name);
  }
  return handshake;
}
inline void* esp_tls_get_ssl_context(esp_tls_t*) { return &ssl; }
inline size_t writeLimit = SIZE_MAX, writeFailAfter = SIZE_MAX;
inline int esp_tls_conn_write(esp_tls_t* t, const void* p, size_t n) {
  if (writeFailAfter == 0) return -1;
  if (writeFailAfter != SIZE_MAX) --writeFailAfter;
  n = std::min(n, writeLimit);
  writes[t->index].append(static_cast<const char*>(p), n);
  return static_cast<int>(n);
}
inline int esp_tls_conn_read(esp_tls_t* t, void* p, size_t n) {
  n = std::min(n, t->reply.size() - t->pos);
  memcpy(p, t->reply.data() + t->pos, n);
  t->pos += n;
  return static_cast<int>(n);
}
inline int fake_getaddrinfo(const char*, const char*, const addrinfo* h, addrinfo** out) {
  ++dnsCalls;
  assert(h->ai_family == AF_INET);
  *out = nullptr;
  addrinfo** tail = out;
  for (const auto& ip : dnsAnswers) {
    auto* a = new addrinfo{};
    auto* sa = new sockaddr_in{};
    sa->sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &sa->sin_addr);
    a->ai_family = AF_INET;
    a->ai_addrlen = sizeof(*sa);
    a->ai_addr = reinterpret_cast<sockaddr*>(sa);
    *tail = a;
    tail = &a->ai_next;
  }
  return 0;
}
inline void fake_freeaddrinfo(addrinfo* a) {
  while (a) {
    auto* next = a->ai_next;
    delete reinterpret_cast<sockaddr_in*>(a->ai_addr);
    delete a;
    a = next;
  }
}
#define getaddrinfo fake_getaddrinfo
#define freeaddrinfo fake_freeaddrinfo
