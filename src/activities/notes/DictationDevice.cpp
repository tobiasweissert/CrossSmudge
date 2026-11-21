#if defined(ARDUINO_ARCH_ESP32) && defined(FREEINK_DEVICE_STICKY) && FREEINK_DEVICE_STICKY && !defined(SIMULATOR)
#include "DictationDevice.h"

#include <AppNetworkPolicy.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP_I2S.h>
#include <HalClock.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_tls.h>
#include <mbedtls/build_info.h>
#include <mbedtls/ssl.h>
#include <strings.h>

#include <algorithm>
#include <cmath>
#include <ctime>
namespace dictation {
namespace {
char sessionKey[maxKey + 1]{};
std::atomic<uint32_t> keyGeneration{1};
constexpr char boundary[] = "StickyNote7nzAx3";
constexpr char prefix[] =
    "--StickyNote7nzAx3\r\nContent-Disposition: form-data; "
    "name=\"model\"\r\n\r\ngpt-transcribe\r\n--StickyNote7nzAx3\r\nContent-Disposition: form-data; name=\"file\"; "
    "filename=\"note.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
constexpr char tail[] = "\r\n--StickyNote7nzAx3--\r\n";
struct Channel {
  esp_tls_t* h = esp_tls_init();
  ~Channel() {
    if (h) esp_tls_conn_destroy(h);
  }
};
bool networkAllowed() {
  bool ok = false;
  return ink::http::networkEnabled(ok) && ok;
}
int compare(const mbedtls_x509_time& x, const tm& t) {
  int a[] = {x.year, x.mon, x.day, x.hour, x.min, x.sec},
      b[] = {1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec};
  for (unsigned i = 0; i < 6; ++i)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}
bool verified(Channel& c, time_t epoch) {
  tm utc{};
  if (!gmtime_r(&epoch, &utc)) return false;
  auto ssl = static_cast<mbedtls_ssl_context*>(esp_tls_get_ssl_context(c.h));
  if (!ssl || mbedtls_ssl_get_verify_result(ssl)) return false;
  auto cert = mbedtls_ssl_get_peer_cert(ssl);
  if (!cert) return false;
  unsigned count = 0;
  for (; cert; cert = cert->next) {
    if (++count > 8 || compare(cert->valid_from, utc) > 0 || compare(cert->valid_to, utc) < 0) return false;
  }
  return true;
}
const char* upload(Job& job, const char* key, uint8_t* audio, size_t size) {
#if !defined(MBEDTLS_HAVE_TIME_DATE) || !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  return "Firmware lacks verified certificate time or roots.";
#else
  HalClock::SyncSample sample{};
  if (!networkAllowed() || !WiFi.isConnected() || !halClock.syncSystemTimeFromNTP(&sample))
    return "Sync the device clock first.";
  if (std::search(audio, audio + size, boundary, boundary + sizeof(boundary) - 1) != audio + size)
    return "Record again (multipart collision).";
  const time_t anchor = sample.epoch;
  const uint32_t began = sample.monotonicMs;
  if (anchor < 1735689600 || anchor > 4102444800LL) return "Sync the device clock first.";
  Channel c;
  if (!c.h) return "TLS client unavailable.";
  esp_tls_cfg_t cfg{};
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 4000;
  constexpr char host[] = "api.openai.com";
  auto now = [&] { return anchor + (millis() - began) / 1000; };
  auto live = [&] {
    return job.authorized() && !job.cancelled && millis() - began < 20000 &&
           std::abs(difftime(time(nullptr), now())) <= 1;
  };
  if (!live() || esp_tls_conn_new_sync(host, sizeof(host) - 1, 443, &cfg, c.h) != 1 || !verified(c, now()) ||
      !networkAllowed() || !live())
    return "Secure connection failed.";
  auto write = [&](const char* p, size_t n) {
    while (n && live()) {
      int k = esp_tls_conn_write(c.h, p, std::min<size_t>(n, 1024));
      if (k <= 0) return false;
      p += k;
      n -= k;
    }
    return !n;
  };
  char header[maxKey + 256];
  int count =
      snprintf(header, sizeof(header),
               "POST /v1/audio/transcriptions HTTP/1.0\r\nHost: api.openai.com\r\nConnection: close\r\nAuthorization: "
               "Bearer %s\r\nContent-Type: multipart/form-data; boundary=%s\r\nContent-Length: %zu\r\n\r\n",
               key, boundary, sizeof(prefix) - 1 + size + sizeof(tail) - 1);
  bool sent = count > 0 && size_t(count) < sizeof(header) && write(header, count);
  wipe(header, sizeof(header));
  if (!sent || !write(prefix, sizeof(prefix) - 1) || !write(reinterpret_cast<char*>(audio), size) ||
      !write(tail, sizeof(tail) - 1))
    return "Upload interrupted. Not retried.";
  // Reuse the spent PSRAM recording for the response. All HTTP bytes count
  // toward an aggregate cap, including slow-dripped headers and error bodies.
  char* raw = reinterpret_cast<char*>(audio);
  size_t used = 0;
  constexpr size_t cap = 8192;
  while (live()) {
    int n = esp_tls_conn_read(c.h, raw + used, std::min<size_t>(512, cap + 1 - used));
    if (n == 0) break;
    if (n < 0) return "Response read failed.";
    used += n;
    if (used > cap) return "Response too large.";
  }
  if (!live() || memchr(raw, 0, used)) return "Incomplete response.";
  raw[used] = 0;
  char* end = strstr(raw, "\r\n\r\n");
  char* line = strstr(raw, "\r\n");
  if (!end || !line || line - raw < 12 || strncmp(raw, "HTTP/1.", 7) || (raw[7] != '0' && raw[7] != '1') ||
      raw[8] != ' ' || raw[9] < '1' || raw[9] > '5' || raw[10] < '0' || raw[10] > '9' || raw[11] < '0' ||
      raw[11] > '9' || (raw + 12 != line && raw[12] != ' '))
    return "Malformed response.";
  if (auto err = statusError((raw[9] - '0') * 100 + (raw[10] - '0') * 10 + raw[11] - '0')) return err;
  char* body = end + 4;
  size_t bodySize = used - (body - raw);
  if (!bodySize || bodySize > 4096) return "Response too large or empty.";
  bool gotLength = false;
  for (char* p = line + 2; p < end; p = line + 2) {
    line = strstr(p, "\r\n");
    if (!line) return "Malformed headers.";
    *line = 0;
    if (!strncasecmp(p, "Transfer-Encoding:", 18)) return "Unsupported transfer encoding.";  // HTTP/1.0
    if (!strncasecmp(p, "Content-Length:", 15)) {
      char* v = p + 15;
      while (*v == ' ' || *v == '\t') ++v;
      size_t len = 0;
      if (!*v || gotLength) return "Invalid length.";
      for (; *v; ++v) {
        if (*v < '0' || *v > '9' || len > 4096) return "Invalid length.";
        len = len * 10 + (*v - '0');
      }
      if (len != bodySize) return "Incomplete response.";
      gotLength = true;
    }
  }
  JsonDocument doc;
  if (deserializeJson(doc, body, bodySize) || !doc["text"].is<const char*>()) return "Malformed response.";
  JsonString text = doc["text"].as<JsonString>();
  if (!text.size() || text.size() > maxText || memchr(text.c_str(), 0, text.size())) return "Invalid transcript size.";
  job.transcript.assign(text.c_str(), text.size());
  return nullptr;
#endif
}
}  // namespace
bool provisioningAvailable() {
#if defined(MBEDTLS_HAVE_TIME_DATE) && CONFIG_MBEDTLS_CERTIFICATE_BUNDLE && CONFIG_ESP_COREDUMP_ENABLE_TO_NONE && \
    CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT && !CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH &&                              \
    !CONFIG_ESP_COREDUMP_ENABLE_TO_UART
  return true;
#else
  return false;
#endif
}
bool configured() { return sessionKey[0] != 0; }
bool Job::authorized() const { return keyGeneration.load() == generation; }
void forget() {
  ++keyGeneration;
  wipe(sessionKey, sizeof(sessionKey));
}
bool configure(const std::string& value) {
  if (!provisioningAvailable() || !validKey(value.data(), value.size())) return false;
  forget();
  memcpy(sessionKey, value.data(), value.size());
  return true;
}
bool Job::start() {
  if (running) return false;
  if (!configured() || !WiFi.isConnected() || !networkAllowed()) {
    error = "Set key, connect Wi-Fi and sync clock.";
    state = Failed;
    return false;
  }
  cancelled = false;
  stop = false;
  send = false;
  transcript.clear();
  error = nullptr;
  generation = keyGeneration.load();
  memcpy(key, sessionKey, sizeof(key));
  running = true;
  state = Recording;
  if (xTaskCreate(
          [](void* p) {
            auto& j = *static_cast<Job*>(p);
            j.work();
            wipe(j.key, sizeof(j.key));
            j.running = false;
            vTaskDelete(nullptr);
          },
          "note-voice", 16384, this, 1, nullptr) != pdPASS) {
    wipe(key, sizeof(key));
    error = "Worker unavailable.";
    state = Failed;
    running = false;
    return false;
  }
  return true;
}
void Job::drain() {
  cancelled = true;
  while (running) vTaskDelay(1);
  if (!transcript.empty()) {
    wipe(transcript.data(), transcript.size());
    transcript.clear();
  }
}
void Job::work() {
  auto buf = static_cast<uint8_t*>(heap_caps_malloc(maxPcm + 44, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  size_t n = 0;
  if (!buf) error = "Recording memory unavailable.";
  if (buf && authorized() && !cancelled && !stop) {
    I2SClass mic;
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    delay(20);
    mic.setPinsPdmRx(19, 20);
    mic.setTimeout(40);
    if (!cancelled && !stop && mic.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
      uint32_t start = millis();
      while (authorized() && !cancelled && !stop && millis() - start < 5000 && n < maxPcm) {
        size_t k = mic.readBytes(reinterpret_cast<char*>(buf + 44 + n), std::min<size_t>(256, maxPcm - n));
        if (!k || k % 2) {
          error = "Microphone read failed.";
          break;
        }
        n += k;
      }
      mic.end();
    }
    digitalWrite(38, LOW);
  }
  if (!error && (!n || !wav(buf, n))) error = "No audio captured.";
  if (!error && !cancelled) {
    state = Ready;
    uint32_t start = millis();
    while (authorized() && !cancelled && !send && millis() - start < 45000) vTaskDelay(pdMS_TO_TICKS(20));
    if (!send)
      error = "Recording expired.";
    else if (!authorized())
      error = "Key changed. Record again.";
    else if (!cancelled) {
      state = Sending;
      error = upload(*this, key, buf, n + 44);
    }
  }
  if (buf) {
    wipe(buf, maxPcm + 44);
    heap_caps_free(buf);
  }
  state = cancelled ? Idle : error ? Failed : Review;
}
}  // namespace dictation
#endif
