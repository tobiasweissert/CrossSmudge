#pragma once
// Peripherals/scheduler/TLS/JSON-library stand-ins, not cryptographic or hardware evidence.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
inline uint32_t nowMs;
inline int micStarts, posts, cleaned, frees, power, status = 200;
inline bool cert = true;
inline std::string response = R"({"text":"Neutral"})", wire;
inline size_t offset, declared;
inline std::function<void()> task, tick;
inline unsigned millis() { return nowMs; }
inline void delay(int n) { nowMs += n; }
inline void vTaskDelay(int n) {
  delay(n);
  if (tick) tick();
}
inline constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, I2S_MODE_PDM_RX = 1, I2S_DATA_BIT_WIDTH_16BIT = 16,
                     I2S_SLOT_MODE_MONO = 1;
inline constexpr int MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2, ESP_OK = 0, HTTP_METHOD_POST = 4, pdPASS = 1;
inline void pinMode(int p, int m) { assert(p == 38 && m == OUTPUT); }
inline void digitalWrite(int p, int v) {
  assert(p == 38);
  power = v;
}
inline int pdMS_TO_TICKS(int n) { return n; }
inline void vTaskDelete(void*) {}
inline int xTaskCreate(void (*f)(void*), const char*, int, void* arg, int, void*) {
  task = [=] { f(arg); };
  return pdPASS;
}
inline void* heap_caps_malloc(size_t n, int flags) {
  assert(n == 160044 && flags == 3);
  return malloc(n);
}
inline void heap_caps_free(void* p) {
  ++frees;
  free(p);
}
struct I2SClass {
  void setPinsPdmRx(int c, int d) { assert(c == 19 && d == 20); }
  void setTimeout(int) {}
  bool begin(int, int rate, int, int) {
    assert(power && rate == 16000);
    ++micStarts;
    return true;
  }
  void end() {}
  size_t readBytes(char* p, size_t n) {
    memset(p, 1, n);
    return n;
  }
};
inline constexpr int WIFI_OFF = 0, WIFI_STA = 1;
inline struct Wifi {
  int radio = WIFI_STA;
  int getMode() { return radio; }
  void mode(int v) { radio = v; }
  void scanDelete() {}
  bool connected = true;
  bool isConnected() { return connected; }
  void disconnect(bool, bool = false) { connected = false; }
} WiFi;
namespace trustedtime {
inline long trustedNow() { return time(nullptr) - 1; }
}  // namespace trustedtime
using esp_transport_handle_t = void*;
inline constexpr int ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN = -1;
inline std::string reply;
inline int esp_crt_bundle_attach(void*) { return 0; }
inline void* esp_transport_ssl_init() { return &power; }
inline void esp_transport_ssl_crt_bundle_attach(void*, int (*cb)(void*)) { assert(cb == esp_crt_bundle_attach); }
inline int esp_transport_connect(void*, const char* host, int port, int) {
  assert(std::string(host) == "api.openai.com" && port == 443);
  if (!cert) return -1;
  ++posts;
  wire.clear();
  offset = 0;
  reply = "HTTP/1.0 " + std::to_string(status) + " OK\r\nContent-Length: " + std::to_string(response.size()) +
          "\r\n\r\n" + response;
  return 0;
}
inline int esp_transport_write(void*, const char* p, size_t n, int) {
  wire.append(p, n);
  return n;
}
inline int esp_transport_read(void*, char* p, size_t n, int) {
  assert(wire.find("POST /v1/audio/transcriptions HTTP/1.0\r\nHost: api.openai.com\r\n") == 0 &&
         wire.find("Authorization: Bearer fixture-neutral\r\n") != wire.npos &&
         wire.find("gpt-transcribe") != wire.npos && wire.find("RIFF") != wire.npos);
  if (offset == reply.size()) return -1;
  n = std::min(n, reply.size() - offset);
  memcpy(p, reply.data() + offset, n);
  offset += n;
  return n;
}
inline void esp_transport_destroy(void*) { ++cleaned; }
namespace ink::http {
inline bool policy = true, readOK = true;
inline bool networkEnabled(bool& ok) {
  ok = readOK;
  return policy;
}
}  // namespace ink::http
struct HalClock {
  struct SyncSample {
    uint32_t epoch, monotonicMs;
  };
  bool synced = true;
  int skew = 0;
  bool syncSystemTimeFromNTP(SyncSample* s) {
    *s = {uint32_t(time(nullptr) + skew), millis()};
    return synced;
  }
};
inline HalClock halClock;
using esp_tls_t = int;
struct esp_tls_cfg_t {
  int (*crt_bundle_attach)(void*);
  int timeout_ms;
};
inline esp_tls_t* esp_tls_init() { return &power; }
inline void esp_tls_conn_destroy(esp_tls_t* h) { esp_transport_destroy(h); }
inline int esp_tls_conn_new_sync(const char* host, int n, int port, esp_tls_cfg_t* cfg, esp_tls_t* h) {
  assert(n == 14 && cfg->crt_bundle_attach == esp_crt_bundle_attach);
  return esp_transport_connect(h, host, port, 0) == 0 ? 1 : 0;
}
inline int esp_tls_conn_write(esp_tls_t* h, const char* p, size_t n) { return esp_transport_write(h, p, n, 0); }
inline int esp_tls_conn_read(esp_tls_t* h, char* p, size_t n) {
  int r = esp_transport_read(h, p, n, 0);
  return r == -1 ? 0 : r;
}
struct mbedtls_x509_time {
  int year, mon, day, hour, min, sec;
};
struct Certificate {
  mbedtls_x509_time valid_from{2020, 1, 1, 0, 0, 0}, valid_to{2100, 1, 1, 0, 0, 0};
  Certificate* next = nullptr;
};
inline Certificate chain;
using mbedtls_ssl_context = int;
inline void* esp_tls_get_ssl_context(int* h) { return h; }
inline int mbedtls_ssl_get_verify_result(int*) { return 0; }
inline Certificate* mbedtls_ssl_get_peer_cert(int*) { return &chain; }
