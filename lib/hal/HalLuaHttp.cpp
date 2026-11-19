#include "HalLuaHttp.h"

#include "HalAirDropRadio.h"
#ifndef SIMULATOR
#include <AppNetworkPolicy.h>
#include <Arduino.h>
#include <HalClock.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_tls.h>
#include <lwip/netdb.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <atomic>
#include <cstring>
#include <ctime>

namespace {
std::atomic<TaskHandle_t> budgetTask{nullptr};
ink::http::AllocationBudget* activeBudget = nullptr;
std::atomic<TaskHandle_t> receiverBudgetTask{nullptr};
ink::http::AllocationBudget* receiverBudget = nullptr;
portMUX_TYPE timeMux = portMUX_INITIALIZER_UNLOCKED;
int64_t trustedEpoch = 0, trustedMono = 0;
// Pin one synchronized anchor; later wall-clock changes cannot move TLS time.
int64_t trustedNow() {
  portENTER_CRITICAL(&timeMux);
  auto epoch = trustedEpoch;
  auto mono = trustedMono;
  portEXIT_CRITICAL(&timeMux);
  return epoch ? epoch + (esp_timer_get_time() - mono) / 1000000 : 0;
}
bool pinTime(HalLuaHttp::TimeProposal proposal) {
  const auto elapsed = esp_timer_get_time() - proposal.monotonicUs;
  if (proposal.epoch < 1735689600 || proposal.epoch > 4102444800LL || elapsed < 0 || elapsed > 300000000) return false;
  portENTER_CRITICAL(&timeMux);
  trustedMono = proposal.monotonicUs;
  trustedEpoch = proposal.epoch;
  portEXIT_CRITICAL(&timeMux);
  return true;
}
bool prepareTime(ink::http::Job& job) {
  if (job.cancel.load()) {
    job.result.error = "cancelled";
    return false;
  }
  if (trustedNow()) return true;
  // Runs in the existing HTTP worker, keeping the UI responsive during NTP.
  if (!halClock.hasSynchronizedSystemTime() && !halClock.syncSystemTimeFromNTP()) {
    job.result.error = "time_sync_failed";
    return false;
  }
  if (job.cancel.load()) {
    job.result.error = "cancelled";
    return false;
  }
  if (!pinTime(HalLuaHttp::proposeTime())) {
    job.result.error = "time_sync_failed";
    return false;
  }
  return true;
}
int compare(const mbedtls_x509_time& a, const tm& b) {
  const int x[] = {a.year, a.mon, a.day, a.hour, a.min, a.sec};
  const int y[] = {b.tm_year + 1900, b.tm_mon + 1, b.tm_mday, b.tm_hour, b.tm_min, b.tm_sec};
  for (int i = 0; i < 6; ++i)
    if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}
struct Connection {
  esp_tls_t* tls = nullptr;
  const std::atomic<bool>* cancel = nullptr;
  uint32_t start = 0, timeout = 0;
  const char* failure = "tcp_connect";
  int id = 0;
};
bool connect(void* p, const char* host, uint32_t timeout, const std::atomic<bool>& cancel) {
  auto& c = *static_cast<Connection*>(p);
  c.cancel = &cancel;
  c.start = millis();
  c.timeout = timeout;
  if (!trustedNow()) {
    c.failure = "time_untrusted";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    c.failure = "wifi_disconnected";
    return false;
  }
  // TLS needs two 16 KiB records plus certificates, stack and WiFi. Fail before creating it on a crowded C3.
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 112 * 1024 ||
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 40 * 1024) {
    c.failure = "tls_heap";
    LOG_ERR("LHTTP", "TLS admission free=%u largest=%u",
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    return false;
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* addresses = nullptr;
  c.failure = "dns_failed";
  const int dns = getaddrinfo(host, "443", &hints, &addresses);
  if (dns != 0 || !addresses) {
    LOG_ERR("LHTTP", "job=%d stage=DNS code=%d elapsed=%u", c.id, dns, unsigned(millis() - c.start));
    if (addresses) freeaddrinfo(addresses);
    return false;
  }
  uint32_t chosen = 0;
  bool valid = true;
  for (auto* a = addresses; a; a = a->ai_next) {
    if (a->ai_family != AF_INET || a->ai_addrlen < sizeof(sockaddr_in)) {
      valid = false;
      break;
    }
    auto ip = reinterpret_cast<sockaddr_in*>(a->ai_addr)->sin_addr.s_addr;
    if (!ink::http::publicV4(ntohl(ip))) {
      valid = false;
      break;
    }
    if (!chosen) chosen = ip;
  }
  freeaddrinfo(addresses);
  if (!valid || !chosen) {
    c.failure = "address_denied";
    return false;
  }
  if (cancel.load() || uint32_t(millis() - c.start) >= timeout) {
    c.failure = cancel.load() ? "cancelled" : "timeout";
    return false;
  }
  char numeric[16];
  in_addr addr{};
  addr.s_addr = chosen;
  if (!inet_ntop(AF_INET, &addr, numeric, sizeof(numeric))) return false;
  c.failure = "tls_setup";
  c.tls = esp_tls_init();
  if (!c.tls) return false;
  esp_tls_cfg_t cfg{};
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.common_name = host;  // mbedTLS verifies this hostname AND sends it as SNI, while TCP uses numeric only.
  cfg.tls_version = ESP_TLS_VER_TLS_1_2;
  cfg.timeout_ms = 1000;
  cfg.non_block = true;
  int rc;
  do {
    if (cancel.load() || uint32_t(millis() - c.start) >= timeout) {
      c.failure = cancel.load() ? "cancelled" : "timeout";
      return false;
    }
    rc = esp_tls_conn_new_async(numeric, strlen(numeric), 443, &cfg, c.tls);
    if (rc == 0) vTaskDelay(pdMS_TO_TICKS(10));
  } while (rc == 0);
  if (rc != 1) {
    c.failure = "tls_handshake";
    esp_tls_error_handle_t handle = nullptr;
    int code = 0, flags = 0;
    esp_err_t error = ESP_OK;
    if (esp_tls_get_error_handle(c.tls, &handle) == ESP_OK && handle)
      error = esp_tls_get_and_clear_last_error(handle, &code, &flags);
    LOG_ERR("LHTTP", "job=%d stage=TLS rc=%d esp=%d tls=%d verify=%x elapsed=%u", c.id, rc, int(error), code, flags,
            unsigned(millis() - c.start));
    return false;
  }
  c.failure = "tls_verify";
  auto* ssl = static_cast<mbedtls_ssl_context*>(esp_tls_get_ssl_context(c.tls));
  if (!ssl || mbedtls_ssl_get_verify_result(ssl) != 0) return false;
  // Arduino's prebuilt mbedTLS omits HAVE_TIME_DATE: explicitly check every supplied chain certificate.
  auto* cert = mbedtls_ssl_get_peer_cert(ssl);
  if (!cert) return false;
  time_t now = trustedNow();
  tm utc{};
  if (!now || !gmtime_r(&now, &utc)) return false;
  c.failure = "tls_date";
  size_t count = 0, total = 0;
  for (; cert; cert = cert->next) {
    total += cert->raw.len;
    if (++count > 8 || total > 32768) {
      c.failure = "tls_chain_limit";
      return false;
    }
    if (compare(cert->valid_from, utc) > 0 || compare(cert->valid_to, utc) < 0) return false;
  }
  return !cancel.load();
}
int transfer(Connection& c, char* data, size_t size, bool write) {
  for (;;) {
    if (c.cancel->load() || uint32_t(millis() - c.start) >= c.timeout) return -1;
    int n = write ? esp_tls_conn_write(c.tls, data, size) : esp_tls_conn_read(c.tls, data, size);
    if (n != ESP_TLS_ERR_SSL_WANT_READ && n != ESP_TLS_ERR_SSL_WANT_WRITE) return n;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
int read(void* p, char* data, size_t n) { return transfer(*static_cast<Connection*>(p), data, n, false); }
int write(void* p, const char* data, size_t n) {
  return transfer(*static_cast<Connection*>(p), const_cast<char*>(data), n, true);
}
void close(void* p) {
  auto& c = *static_cast<Connection*>(p);
  if (c.tls) {
    esp_tls_conn_destroy(c.tls);
    c.tls = nullptr;
  }
}
uint32_t clockMs(void*) { return millis(); }
void execute(void*, ink::http::Job& job) {
  if (!prepareTime(job)) return;
  activeBudget = &job.tlsBudget;
  budgetTask.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
  Connection c;
  c.id = job.id;
  ink::http::Io io{&c, connect, read, write, close, clockMs};
  ink::http::exchange(job.request, job.result, job.scratch, io, job.cancel);
  if (job.request.browser && job.result.error && strcmp(job.result.error, "tls_or_network") == 0)
    job.result.error = c.failure;
  if (job.cancel.load()) job.result.error = "cancelled";
  budgetTask.store(nullptr, std::memory_order_release);
  activeBudget = nullptr;
  if (job.tlsBudget.used()) LOG_ERR("LHTTP", "TLS allocations retained after close");
  if (job.result.error) LOG_ERR("LHTTP", "Request failed: %s", job.result.error);
}
struct Launch {
  void (*fn)(void*);
  void* arg;
};
// Single service slot means one launch structure; worker copies it before publishing completion.
Launch launch{};
void task(void*) {
  auto fn = launch.fn;
  auto arg = launch.arg;
  fn(arg);
  vTaskDelete(nullptr);
}
bool spawn(void (*fn)(void*), void* arg) {
  launch = {fn, arg};
  // 6 KiB accommodates TLS certificate parsing; no request/body arrays on this stack.
  if (xTaskCreate(task, "lua_https", 6144, nullptr, 1, nullptr) != pdPASS) {
    LOG_ERR("LHTTP", "Worker allocation failed");
    return false;
  }
  return true;
}
}  // namespace

// Link wrappers budget the Lua and AirDrop workers; other TLS callers retain their allocator.
extern "C" void* __real_esp_mbedtls_mem_calloc(size_t, size_t);
extern "C" void __real_esp_mbedtls_mem_free(void*);
extern "C" void* __wrap_esp_mbedtls_mem_calloc(size_t n, size_t size) {
  if (!size || n > SIZE_MAX / size) return nullptr;
  auto owner = budgetTask.load(std::memory_order_acquire);
  const bool limited = owner && owner == xTaskGetCurrentTaskHandle();
  const auto receiverOwner = receiverBudgetTask.load(std::memory_order_acquire);
  const bool receiverLimited = receiverOwner && receiverOwner == xTaskGetCurrentTaskHandle();
  auto* selected = limited ? activeBudget : (receiverLimited ? receiverBudget : nullptr);
  if (selected && !selected->permits(n * size)) return nullptr;
  // Sticky-only AirDrop TLS owns these bounded, non-DMA buffers until mbedTLS
  // frees them. Keep internal RAM available for the radio and task stacks.
  void* p = receiverLimited ? heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                            : __real_esp_mbedtls_mem_calloc(n, size);
  if (selected && p && !selected->add(p, heap_caps_get_allocated_size(p))) {
    __real_esp_mbedtls_mem_free(p);
    return nullptr;
  }
  return p;
}
extern "C" void __wrap_esp_mbedtls_mem_free(void* p) {
  auto owner = budgetTask.load(std::memory_order_acquire);
  if (owner && owner == xTaskGetCurrentTaskHandle()) activeBudget->remove(p);
  const auto receiverOwner = receiverBudgetTask.load(std::memory_order_acquire);
  if (receiverOwner && receiverOwner == xTaskGetCurrentTaskHandle()) receiverBudget->remove(p);
  __real_esp_mbedtls_mem_free(p);
}

namespace HalAirDropRadio {
bool beginTlsBudget(ink::http::AllocationBudget* budget) {
  if (!budget || receiverBudgetTask.load(std::memory_order_acquire)) return false;
  receiverBudget = budget;
  receiverBudgetTask.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
  return true;
}
void endTlsBudget() {
  receiverBudgetTask.store(nullptr, std::memory_order_release);
  receiverBudget = nullptr;
}
}  // namespace HalAirDropRadio

namespace HalLuaHttp {
bool networkEnabled() {
  bool readOk;
  const bool enabled = ink::http::networkEnabled(readOk);
  if (!readOk) LOG_ERR("LHTTP", "Cannot read app network setting; networking disabled");
  return enabled;
}
bool setNetworkEnabled(bool enabled) {
  const bool ok = ink::http::setNetworkEnabled(enabled);
  if (!ok) LOG_ERR("LHTTP", "Cannot save app network setting");
  return ok;
}
TimeProposal proposeTime() { return {time(nullptr), esp_timer_get_time()}; }
bool trustTime(TimeProposal proposal) { return !ink::http::Session::busy() && pinTime(proposal); }
bool timeTrusted() { return trustedNow() != 0; }
bool wifiConnected() { return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0); }
void disconnectWifi() {
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
  }
}
}  // namespace HalLuaHttp
namespace ink::http {
Executor platformExecutor() { return {nullptr, execute, spawn}; }
}  // namespace ink::http
#endif
