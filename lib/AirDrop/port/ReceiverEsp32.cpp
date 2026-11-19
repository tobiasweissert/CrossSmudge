#include <AppCapabilities.h>

#include <cstring>

#include "../Receiver.h"
#if CROSSINK_APP_CAP_AIRDROP
#include <AllocationBudget.h>
#include <Arduino.h>
#include <HalAirDropRadio.h>
#include <Logging.h>
#include <LibraryBuilder.h>
#include <Memory.h>
#include <errno.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <fcntl.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <session/Session.h>
#include <storage/HalStorageSink.h>
#include <vendor/ESP32Drop/src/airdrop/core/airdrop_cert.h>
#include <vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.h>

#include <atomic>

#include "Discovery.h"
namespace airdrop {
namespace {
std::atomic<bool> running{false}, stop{false};
std::atomic<TaskHandle_t> workerHandle{nullptr};
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
Status status;
uint64_t generation = 0;
int decisionValue = 0;
bool decisionResolved = false;
OfferIdentity decisionToken;
uint64_t clockMs() { return uint64_t(esp_timer_get_time() / 1000); }
bool equal(const OfferIdentity& a, const OfferIdentity& b) {
  return a.generation == b.generation && a.session == b.session && a.offer == b.offer && a.connection == b.connection &&
         a.interfaceIndex == b.interfaceIndex && memcmp(a.peer, b.peer, 16) == 0;
}
void state(State next) {
  portENTER_CRITICAL(&statusMux);
  status.state = next;
  ++status.revision;
  portEXIT_CRITICAL(&statusMux);
}
struct Context {
  session::Workspace protocol;
  HeapByteBuffer ask;
  storage::HalStorageSink storage;
  ink::http::AllocationBudget budget;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_x509_crt certificate;
  mbedtls_pk_context key;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context random;
  mbedtls_net_context net;
  int listener = -1, client = -1;
  AwdlIdentity address{};
  AwdlDiag linkStats{};  // Reuse the owner's PSRAM workspace, not a large task-stack snapshot.
  sockaddr_in6 peer{};
  char interfaceName[8]{};
  uint64_t generation = 0, connections = 0, offers = 0, lastDraw = 0;
  bool tlsInitialized = false, budgetBound = false, failed = false, url = false, fileCommitted = false,
       radioOwned = false, discoveryOwned = false;
};
// The session's ~50KiB decoder/parser workspace must not use a 32KiB TLS stack.
// Allocate once in S3 PSRAM, retain through stop, and explicitly destroy after join.
HeapByteBuffer contextMemory;
Context* context = nullptr;
void failure(Context& c, const char* reason) {
  LOG_ERR("AIRDROP", "%s", reason);
  c.failed = true;
  state(State::Error);
}
void closeFd(int& fd) {
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}
void rejectQueued(Context& c) {
  if (c.listener < 0) return;
  for (unsigned i = 0; i < 4; ++i) {
    int fd = accept(c.listener, nullptr, nullptr);
    if (fd < 0) break;
    close(fd);
  }
}
bool nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
void wait(Context& c) {
  rejectQueued(c);
  vTaskDelay(pdMS_TO_TICKS(10));
}
bool alive(void* p) {
  auto& c = *static_cast<Context*>(p);
  uint8_t byte;
  int n = recv(c.client, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
  return n > 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
}
int read(void* p, uint8_t* data, size_t size) {
  auto& c = *static_cast<Context*>(p);
  int n = mbedtls_ssl_read(&c.ssl, data, size);
  if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
  return n > 0 ? n : -1;
}
bool write(void* p, const uint8_t* data, size_t length, uint64_t deadline) {
  auto& c = *static_cast<Context*>(p);
  size_t at = 0;
  while (at < length && !stop.load(std::memory_order_acquire) && clockMs() < deadline) {
    int n = mbedtls_ssl_write(&c.ssl, data + at, length - at);
    if (n > 0)
      at += size_t(n);
    else if (n != MBEDTLS_ERR_SSL_WANT_READ && n != MBEDTLS_ERR_SSL_WANT_WRITE)
      return false;
    if (at < length) wait(c);
  }
  return at == length;
}
uint64_t now(void*) { return clockMs(); }
void cooperativeWait(void* p) { wait(*static_cast<Context*>(p)); }
bool cancelled(void*) { return stop.load(std::memory_order_acquire); }
void pending(void*, const Offer& offer, const OfferIdentity& token, uint64_t deadline) {
  portENTER_CRITICAL(&statusMux);
  status.offer = offer;
  status.identity = token;
  status.deadlineMs = deadline;
  status.state = State::Pending;
  decisionValue = 0;
  decisionResolved = false;
  ++status.revision;
  portEXIT_CRITICAL(&statusMux);
  LOG_INF("AIRDROP", "Offer %llu awaiting consent (%u bytes)", static_cast<unsigned long long>(token.offer),
          unsigned(offer.fileSize));
}
int decision(void*, const OfferIdentity& token) {
  int result = 0;
  portENTER_CRITICAL(&statusMux);
  if (decisionValue && equal(token, decisionToken)) {
    result = decisionValue;
    decisionValue = 0;
  }
  portEXIT_CRITICAL(&statusMux);
  return result;
}
bool beginFile(void* p, const Offer& offer) {
  auto& c = *static_cast<Context*>(p);
  state(State::Receiving);
  c.fileCommitted = false;
  c.storage.setDeadlineMs(c.protocol.deadline);
  const bool ok = c.storage.begin(offer.filename, offer.fileSize);
  if (ok)
    LOG_INF("AIRDROP", "SD staging started (%u bytes)", unsigned(offer.fileSize));
  else
    LOG_ERR("AIRDROP", "SD staging failed: %s", c.storage.error());
  return ok;
}
stream::WriteResult append(void* p, const uint8_t* data, size_t size) {
  auto& c = *static_cast<Context*>(p);
  return c.storage.append(data, size) ? stream::WriteResult::Consumed : stream::WriteResult::Error;
}
bool finishFile(void* p) {
  auto& c = *static_cast<Context*>(p);
  LOG_INF("AIRDROP", "Upload archive complete; verifying SD save");
  state(State::Publishing);
  bool ok = c.storage.finish();
  portENTER_CRITICAL(&statusMux);
  if (ok) {
    c.fileCommitted = true;
    status.committedBytes = c.storage.committedBytes();
    strlcpy(status.savedPath, c.storage.savedPath(), sizeof status.savedPath);
    status.state = State::Success;
  } else {
    status.publicationUncertain = c.storage.state() == storage::State::Uncertain;
    status.state = State::Error;
  }
  ++status.revision;
  portEXIT_CRITICAL(&statusMux);
  // Publication owner, outside the status lock: never offer/partial/uncertain/URL receipt.
  if (ok && library::isLibraryBookFile(c.storage.savedPath())) library::markLibraryIndexDirty();
  if (!ok)
    failure(c, c.storage.error());
  else
    LOG_INF("AIRDROP", "Saved %u bytes to %s", unsigned(c.storage.committedBytes()), c.storage.savedPath());
  return ok;
}
void cancelFile(void* p) { static_cast<Context*>(p)->storage.abortTransfer(); }
void progress(void* p, uint64_t wire, uint64_t payload) {
  auto& c = *static_cast<Context*>(p);
  auto t = clockMs();
  if (t - c.lastDraw < 500 && payload != c.protocol.consent.offer().fileSize) return;
  c.lastDraw = t;
  portENTER_CRITICAL(&statusMux);
  status.wireBytes = wire;
  status.payloadBytes = payload;
  status.committedBytes = 0;
  ++status.revision;
  portEXIT_CRITICAL(&statusMux);
}
void urlAccepted(void* p, const Offer&, const OfferIdentity&) {
  auto& c = *static_cast<Context*>(p);
  c.url = true;
  stop.store(true, std::memory_order_release);
}
void protocolFailure(void* p, const char* text) {
  auto& c = *static_cast<Context*>(p);
  LOG_ERR("AIRDROP", "Request failed: %s", text);
  // A bad request or disconnected sender owns only this connection. Storage
  // failures already latch c.failed and publication uncertainty separately.
  if (!c.failed) state(State::Listening);
}
bool preparePsa() {
  // PSA owns a persistent process-wide cache and lazily allocated volatile-slot
  // slice. Initialise them BEFORE task-scoped TLS accounting, without tearing
  // down shared PSA state. Sixteen short-lived raw keys reserve the pinned
  // first slice (16 slots); all temporary keys are explicitly destroyed.
  if (psa_crypto_init() != PSA_SUCCESS) return false;
  static bool prepared = false;
  static psa_key_id_t keys[16]{};  // Retain every owned temporary ID through a failed destroy.
  for (auto& key : keys)
    if (key) {
      if (psa_destroy_key(key) != PSA_SUCCESS) return false;
      key = 0;
    }
  if (prepared) return true;
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attributes, PSA_KEY_TYPE_RAW_DATA);
  psa_set_key_bits(&attributes, 128);
  static const uint8_t material[16] = {};
  bool ok = true;
  for (unsigned i = 0; i < 16; ++i)
    if (psa_import_key(&attributes, material, sizeof material, &keys[i]) != PSA_SUCCESS) {
      ok = false;
      break;
    }
  for (auto& key : keys)
    if (key) {
      if (psa_destroy_key(key) != PSA_SUCCESS)
        ok = false;
      else
        key = 0;
    }
  psa_reset_key_attributes(&attributes);
  prepared = ok;
  return ok;
}
bool initTls(Context& c) {
  if (!preparePsa()) {
    failure(c, "PSA initialization failed");
    return false;
  }
  // The owner-scoped HAL allocator places AirDrop TLS in PSRAM, not the
  // internal heap already occupied by Wi-Fi and the three receiver tasks.
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < 96 * 1024 ||
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < 40 * 1024) {
    failure(c, "TLS PSRAM admission failed");
    return false;
  }
  c.budgetBound = HalAirDropRadio::beginTlsBudget(&c.budget);
  if (!c.budgetBound) {
    failure(c, "TLS budget already owned");
    return false;
  }
  mbedtls_ssl_init(&c.ssl);
  mbedtls_ssl_config_init(&c.config);
  mbedtls_x509_crt_init(&c.certificate);
  mbedtls_pk_init(&c.key);
  mbedtls_entropy_init(&c.entropy);
  mbedtls_ctr_drbg_init(&c.random);
  mbedtls_net_init(&c.net);
  c.tlsInitialized = true;
  static const unsigned char personalization[] = "CrossSmudge AirDrop";
  if (mbedtls_ctr_drbg_seed(&c.random, mbedtls_entropy_func, &c.entropy, personalization, sizeof personalization - 1) ||
      mbedtls_x509_crt_parse_der(&c.certificate, AIRDROP_CRT_EC_DER, AIRDROP_CRT_EC_DER_LEN) ||
      mbedtls_pk_parse_key(&c.key, AIRDROP_KEY_EC_DER, AIRDROP_KEY_EC_DER_LEN, nullptr, 0, mbedtls_ctr_drbg_random,
                           &c.random) ||
      mbedtls_ssl_config_defaults(&c.config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT)) {
    failure(c, "TLS configuration failed");
    return false;
  }
  // Everyone semantics only. This inbound self-signed peer channel never changes
  // Browser's independent outgoing certificate/DNS/date/redirect verification.
  mbedtls_ssl_conf_authmode(&c.config, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&c.config, mbedtls_ctr_drbg_random, &c.random);
  mbedtls_ssl_conf_min_tls_version(&c.config, MBEDTLS_SSL_VERSION_TLS1_2);
  if (mbedtls_ssl_conf_own_cert(&c.config, &c.certificate, &c.key) || mbedtls_ssl_setup(&c.ssl, &c.config)) {
    failure(c, "TLS server allocation failed");
    return false;
  }
  return true;
}
bool bindInterface(int fd, const char* name) {
  struct ifreq request{};
  if (strlen(name) >= sizeof request.ifr_name) return false;
  strlcpy(request.ifr_name, name, sizeof request.ifr_name);
  return setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &request, sizeof request) == 0;
}
bool listen(Context& c) {
  c.listener = socket(AF_INET6, SOCK_STREAM, 0);
  if (c.listener < 0) return false;
  int reuse = 1;
  if (setsockopt(c.listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) != 0) return false;
  sockaddr_in6 local{};
  local.sin6_family = AF_INET6;
  local.sin6_port = htons(8770);
  local.sin6_scope_id = awdl_netif_index();
  memcpy(&local.sin6_addr, c.address.ll, 16);
  if (!local.sin6_scope_id || esp_netif_get_netif_impl_name(awdl_netif(), c.interfaceName) != ESP_OK) return false;
  return bindInterface(c.listener, c.interfaceName) &&
         bind(c.listener, reinterpret_cast<sockaddr*>(&local), sizeof local) == 0 && ::listen(c.listener, 2) == 0 &&
         nonblocking(c.listener);
}
void freeTls(Context& c) {
  closeFd(c.client);
  c.net.fd = -1;
  closeFd(c.listener);
  if (c.tlsInitialized) {
    mbedtls_ssl_free(&c.ssl);
    mbedtls_ssl_config_free(&c.config);
    mbedtls_pk_free(&c.key);
    mbedtls_x509_crt_free(&c.certificate);
    mbedtls_ctr_drbg_free(&c.random);
    mbedtls_entropy_free(&c.entropy);
    c.tlsInitialized = false;
  }
  if (c.budgetBound) {
    HalAirDropRadio::endTlsBudget();
    c.budgetBound = false;
  }
}
void worker(void* p) {
  auto& c = *static_cast<Context*>(p);
  workerHandle.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
  if (stop.load(std::memory_order_acquire)) goto cleanup;
  {
    char token[33];
    bool scanOk = false;
    for (unsigned i = 0; i <= 256; ++i) {
      const bool found = c.storage.recoveryToken(i, token, scanOk);
      // UI cancellation makes checked SD reads fail too. It is not evidence
      // that a previously published candidate needs repair, and must not latch
      // a permanent publication refusal on an otherwise healthy next entry.
      if (stop.load(std::memory_order_acquire)) goto cleanup;
      if (!found) {
        if (!scanOk) {
          failure(c, "SD recovery inventory unreadable");
          goto cleanup;
        }
        break;
      }
      if (i == 256) {
        failure(c, "SD recovery inventory limit");
        goto cleanup;
      }
      auto recovered = c.storage.inspectJournal(token);
      if (stop.load(std::memory_order_acquire)) goto cleanup;
      if (recovered != storage::State::PublishedVerified) {
        recovered = c.storage.inspect(token);
        if (stop.load(std::memory_order_acquire)) goto cleanup;
      }
      if (recovered == storage::State::Uncertain) {
        portENTER_CRITICAL(&statusMux);
        status.publicationUncertain = true;
        portEXIT_CRITICAL(&statusMux);
        failure(c, "SD publication requires recovery");
        goto cleanup;
      }
    }
  }
  if (stop.load(std::memory_order_acquire)) goto cleanup;
  if (!HalAirDropRadio::releaseArduino()) {
    failure(c, "Arduino radio owner not released");
    goto cleanup;
  }
  if (stop.load(std::memory_order_acquire)) goto cleanup;
  if (awdl_begin() != ESP_OK) {
    c.radioOwned = !awdl_is_stopped();
    failure(c, "AWDL acquisition failed");
    goto cleanup;
  }
  c.radioOwned = true;
  awdl_identity(&c.address);
  if (stop.load(std::memory_order_acquire)) goto cleanup;
  if (!initTls(c) || !listen(c) || !(c.discoveryOwned = discovery::start())) {
    failure(c, "Native receiver startup failed");
    goto cleanup;
  }
  state(State::Listening);
  LOG_INF("AIRDROP", "Listening on AWDL port 8770");
  {
    const uint64_t visibleUntil = clockMs() + 600000;
    while (!stop.load(std::memory_order_acquire) && !c.failed && clockMs() < visibleUntil) {
      socklen_t size = sizeof c.peer;
      c.client = accept(c.listener, reinterpret_cast<sockaddr*>(&c.peer), &size);
      if (c.client < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          failure(c, "Listener accept failed");
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
      const auto* peerBytes = reinterpret_cast<const uint8_t*>(&c.peer.sin6_addr);
      if (size != sizeof c.peer || peerBytes[0] != 0xfe || (peerBytes[1] & 0xc0) != 0x80 || !nonblocking(c.client) ||
          !bindInterface(c.client, c.interfaceName)) {
        closeFd(c.client);
        continue;
      }
      {
        const int reset = mbedtls_ssl_session_reset(&c.ssl);
        if (reset) {
          LOG_ERR("AIRDROP", "TLS session reset failed: %d", reset);
          closeFd(c.client);
          continue;
        }
      }
      c.net.fd = c.client;
      mbedtls_ssl_set_bio(&c.ssl, &c.net, mbedtls_net_send, mbedtls_net_recv, nullptr);
      {
        uint64_t deadline = clockMs() + 15000;
        int result = MBEDTLS_ERR_SSL_WANT_READ;
        while (!stop.load() && clockMs() < deadline &&
               (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE)) {
          result = mbedtls_ssl_handshake(&c.ssl);
          if (result) wait(c);
        }
        if (result) {
          LOG_ERR("AIRDROP", "TLS handshake failed: %d", result);
          closeFd(c.client);
          continue;
        }
      }
      c.protocol.identity = {};
      c.protocol.identity.generation = c.generation;
      c.protocol.identity.session = ++c.connections;
      c.protocol.identity.connection = c.connections;
      c.protocol.identity.interfaceIndex = awdl_netif_index();
      memcpy(c.protocol.identity.peer, peerBytes, 16);
      LOG_INF("AIRDROP", "TLS connection %llu ready", static_cast<unsigned long long>(c.connections));
      {
        session::Io io{&c, read, write, alive, now, cooperativeWait};
        session::Backend backend{&c,         cancelled,  pending,  decision,    beginFile,      append,
                                 finishFile, cancelFile, progress, urlAccepted, protocolFailure};
        session::serve(c.protocol, io, backend, c.protocol.identity, c.offers);
      }
      LOG_INF("AIRDROP", "Connection %llu finished; consent=%u", static_cast<unsigned long long>(c.connections),
              unsigned(c.protocol.consent.state()));
      awdl_diag_read(&c.linkStats, false);
      LOG_INF("AIRDROP", "AWDL totals: rx=%u ringDrop=%u tx=%u txDrop=%u retry=%u exhausted=%u",
              unsigned(c.linkStats.netif_rx), unsigned(c.linkStats.ring_drop), unsigned(c.linkStats.netif_tx_sent),
              unsigned(c.linkStats.netif_txq_drop), unsigned(c.linkStats.netif_retry_queued),
              unsigned(c.linkStats.netif_retry_exhausted));
      (void)mbedtls_ssl_close_notify(&c.ssl);
      closeFd(c.client);
      c.net.fd = -1;
      if (!c.failed && !c.url && !stop.load() && status.state != State::Success) state(State::Listening);
    }
  }
cleanup:
  stop.store(true, std::memory_order_release);
  state(State::Stopping);
  c.storage.cancel();
  freeTls(c);
  {
    const uint64_t deadline = clockMs() + 5000;
    while (c.discoveryOwned && !discovery::detach()) {
      if (clockMs() >= deadline) {
        portENTER_CRITICAL(&statusMux);
        status.ownershipUncertain = true;
        status.state = State::Error;
        ++status.revision;
        portEXIT_CRITICAL(&statusMux);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  if (c.radioOwned) {
    awdl_request_stop();
    const uint64_t deadline = clockMs() + 5000;
    while (!awdl_is_stopped()) {
      auto result = awdl_poll_stop();
      if (result != ESP_OK && result != ESP_ERR_TIMEOUT)
        LOG_ERR("AIRDROP", "Radio release retained ownership: %d", result);
      if (clockMs() >= deadline) {
        portENTER_CRITICAL(&statusMux);
        status.ownershipUncertain = true;
        status.state = State::Error;
        ++status.revision;
        portEXIT_CRITICAL(&statusMux);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  if (c.budget.used()) {
    failure(c, "TLS allocations retained after cleanup");
    portENTER_CRITICAL(&statusMux);
    status.ownershipUncertain = true;
    ++status.revision;
    portEXIT_CRITICAL(&statusMux);
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  LOG_INF("AIRDROP", "Stopped heap=%u largest=%u PSRAM=%u stackMin=%u",
          unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
          unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
          unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  portENTER_CRITICAL(&statusMux);
  status.ownershipUncertain = false;
  status.state = c.failed ? State::Error : c.url ? State::UrlReady : c.fileCommitted ? State::Success : State::Stopped;
  ++status.revision;
  portEXIT_CRITICAL(&statusMux);
  workerHandle.store(nullptr, std::memory_order_release);
  running.store(false, std::memory_order_release);
  vTaskDelete(nullptr);
}
}  // namespace
Receiver& Receiver::instance() {
  static Receiver instance;
  return instance;
}
bool Receiver::begin() {
  if (running.load(std::memory_order_acquire)) return false;
  portENTER_CRITICAL(&statusMux);
  bool uncertain = status.publicationUncertain;
  portEXIT_CRITICAL(&statusMux);
  if (uncertain) return false;
  if (context) {
    context->~Context();
    context = nullptr;
    contextMemory.reset();
  }
  contextMemory = makePsramByteBufferNoThrow(sizeof(Context));
  if (!contextMemory) {
    LOG_ERR("AIRDROP", "Session workspace allocation failed: %u", unsigned(sizeof(Context)));
    state(State::Error);
    return false;
  }
  context = new (contextMemory.get()) Context;
  context->ask = makePsramByteBufferNoThrow(MaxAskBytes);
  if (!context->ask) {
    LOG_ERR("AIRDROP", "Ask allocation failed");
    context->~Context();
    context = nullptr;
    contextMemory.reset();
    state(State::Error);
    return false;
  }
  context->protocol.ask = context->ask.get();
  context->generation = ++generation;
  stop.store(false, std::memory_order_release);
  portENTER_CRITICAL(&statusMux);
  status = Status{};
  status.state = State::Starting;
  ++status.revision;
  decisionValue = 0;
  portEXIT_CRITICAL(&statusMux);
  running.store(true, std::memory_order_release);
  TaskHandle_t handle = nullptr;
  // Same bounded 32KiB TLS stack as qualified foundation; no body/codec arrays on it.
  if (xTaskCreatePinnedToCore(worker, "airdrop_tls", 32768, context, 1, &handle, 0) != pdPASS) {
    running.store(false);
    LOG_ERR("AIRDROP", "TLS worker allocation failed");
    state(State::Error);
    return false;
  }
  return true;
}
void Receiver::requestStop() {
  stop.store(true, std::memory_order_release);
  if (running.load(std::memory_order_acquire) && context) context->storage.requestCancel();
}
bool Receiver::stopped() const { return !running.load(std::memory_order_acquire); }
void Receiver::reap() {
  if (!running.load(std::memory_order_acquire) && context) {
    context->~Context();
    context = nullptr;
    contextMemory.reset();
  }
}
void Receiver::snapshot(Status& out) const {
  portENTER_CRITICAL(&statusMux);
  out = status;
  portEXIT_CRITICAL(&statusMux);
  out.nowMs = clockMs();
}
bool Receiver::decide(const OfferIdentity& token, bool accept) {
  bool ok = false;
  portENTER_CRITICAL(&statusMux);
  if (status.state == State::Pending && !decisionResolved && equal(token, status.identity) &&
      clockMs() < status.deadlineMs) {
    decisionToken = token;
    decisionValue = accept ? 1 : -1;
    decisionResolved = true;
    ok = true;
  }
  portEXIT_CRITICAL(&statusMux);
  return ok;
}
bool Receiver::resolvePendingUrl(const OfferIdentity& token, bool) {
  bool ok = false;
  portENTER_CRITICAL(&statusMux);
  if (!running.load() && status.state == State::UrlReady && equal(token, status.identity)) {
    status.offer.url[0] = 0;
    status.state = State::Stopped;
    ++status.revision;
    ok = true;
  }
  portEXIT_CRITICAL(&statusMux);
  return ok;
}
}  // namespace airdrop
#else
namespace airdrop {
Receiver& Receiver::instance() {
  static Receiver instance;
  return instance;
}
bool Receiver::begin() { return false; }
void Receiver::requestStop() {}
bool Receiver::stopped() const { return true; }
void Receiver::reap() {}
void Receiver::snapshot(Status& out) const { out = {}; }
bool Receiver::decide(const OfferIdentity&, bool) { return false; }
bool Receiver::resolvePendingUrl(const OfferIdentity&, bool) { return false; }
}  // namespace airdrop
#endif
