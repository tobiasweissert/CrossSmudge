#include "ChatPost.h"
#include "ChatTrust.h"
#if defined(FREEINK_DEVICE_STICKY)
#include <Arduino.h>
#include <TrustedTime.h>
#include <WiFiClient.h>
#include <lwip/netdb.h>
#include <wolfssl/ssl.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#if defined(NO_ASN_TIME)
#error Chat requires certificate validity date checking
#endif
namespace stickychat {
namespace {
struct TlsSession {
  WiFiClient tcp;
  WOLFSSL_CTX* ctx = nullptr;
  WOLFSSL* tls = nullptr;
  ~TlsSession() {
    if (tls) wolfSSL_free(tls);
    if (ctx) wolfSSL_CTX_free(ctx);
    tcp.stop();
  }
};
int sendIo(WOLFSSL*, char* p, int n, void* user) {
  auto& tcp = *static_cast<WiFiClient*>(user);
  int sent = tcp.write(reinterpret_cast<const uint8_t*>(p), n);
  return sent > 0 ? sent : tcp.connected() ? WOLFSSL_CBIO_ERR_WANT_WRITE : WOLFSSL_CBIO_ERR_CONN_CLOSE;
}
int receiveIo(WOLFSSL*, char* p, int n, void* user) {
  auto& tcp = *static_cast<WiFiClient*>(user);
  if (!tcp.available()) return tcp.connected() ? WOLFSSL_CBIO_ERR_WANT_READ : WOLFSSL_CBIO_ERR_CONN_CLOSE;
  int got = tcp.read(reinterpret_cast<uint8_t*>(p), n);
  return got > 0 ? got : WOLFSSL_CBIO_ERR_WANT_READ;
}
bool want(int code) { return code == WOLFSSL_ERROR_WANT_READ || code == WOLFSSL_ERROR_WANT_WRITE; }
}  // namespace
static PostResult postImpl(const OwnerConfig& owner, Route route, const uint8_t* body, size_t length,
                           const std::atomic<bool>& cancel, bool provider) {
  auto fail = [](const char* reason) {
    PostResult r;
    r.error = reason;
    return r;
  };
  OwnerConfig config = owner;
  if (!validateConfig(config)) return fail("Unconfigured: owner HTTPS URL, CA and device token required");
  if (!body || !length || length > (route == Route::Chat ? kContextMax : kWavMax) ||
      (route == Route::Transcribe && length < 46))
    return fail("Request exceeds protocol bounds");
  if (provider && (config.host != "api.openai.com" || config.allowPrivate || !validProviderKey(config.token)))
    return fail("Invalid provider configuration");
  const int64_t trusted = trustedtime::trustedNow();
  if (!trusted || static_cast<int64_t>(std::time(nullptr)) < trusted)
    return fail("Clock untrusted or rolled back: synchronize time in Settings");
  uint32_t started = millis();
  auto stopped = [&] { return cancel.load() || static_cast<uint32_t>(millis() - started) >= 30000; };
  if (stopped()) return fail("Cancelled");
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* answers = nullptr;
  if (getaddrinfo(config.host.c_str(), "443", &hints, &answers) != 0 || !answers) return fail("Backend DNS failed");
  uint32_t ip = 0;
  bool allowed = true;
  for (auto* a = answers; a; a = a->ai_next) {
    if (a->ai_family != AF_INET || a->ai_addrlen < sizeof(sockaddr_in)) {
      allowed = false;
      break;
    }
    uint32_t address = ntohl(reinterpret_cast<sockaddr_in*>(a->ai_addr)->sin_addr.s_addr);
    if (!allowedIpv4(address, config.allowPrivate)) {
      allowed = false;
      break;
    }
    if (!ip) ip = address;
  }
  freeaddrinfo(answers);
  if (!allowed || !ip) return fail("Backend address rejected by owner-host policy");
  if (stopped()) return fail("Cancelled or timed out");
  TlsSession s;
  s.tcp.setConnectionTimeout(1000);
  s.tcp.setTimeout(100);
  if (!s.tcp.connect(IPAddress(ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255), 443))
    return fail("Backend connection failed");
  if (stopped()) return fail("Cancelled or timed out");
  wolfSSL_Init();
  s.ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
  if (!s.ctx) return fail("TLS allocation failed");
  if (wolfSSL_CTX_SetMinVersion(s.ctx, WOLFSSL_TLSV1_2) != WOLFSSL_SUCCESS) return fail("TLS policy failed");
  wolfSSL_CTX_set_verify(s.ctx, WOLFSSL_VERIFY_PEER, nullptr);
  if (wolfSSL_CTX_load_verify_buffer(s.ctx, reinterpret_cast<const unsigned char*>(config.ca.data()), config.ca.size(),
                                     WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS)
    return fail("Owner CA invalid");
  if (provider) {
    for (const char* root : {kChatGtsR1, kChatGtsR2, kChatGtsR3, kChatGtsR4}) {
      if (wolfSSL_CTX_load_verify_buffer(s.ctx, reinterpret_cast<const unsigned char*>(root), strlen(root),
                                        WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS)
        return fail("Provider trust unavailable");
    }
  }
  wolfSSL_SetIOSend(s.ctx, sendIo);
  wolfSSL_SetIORecv(s.ctx, receiveIo);
  s.tls = wolfSSL_new(s.ctx);
  if (!s.tls) return fail("TLS allocation failed");
  wolfSSL_SetIOReadCtx(s.tls, &s.tcp);
  wolfSSL_SetIOWriteCtx(s.tls, &s.tcp);
  if (wolfSSL_check_domain_name(s.tls, config.host.c_str()) != WOLFSSL_SUCCESS ||
      wolfSSL_UseSNI(s.tls, WOLFSSL_SNI_HOST_NAME, config.host.c_str(), config.host.size()) != WOLFSSL_SUCCESS)
    return fail("TLS hostname policy failed");
  int n;
  while ((n = wolfSSL_connect(s.tls)) != WOLFSSL_SUCCESS) {
    if (!want(wolfSSL_get_error(s.tls, n))) return fail("TLS certificate or handshake rejected");
    if (stopped()) return fail("Cancelled or timed out");
    delay(5);
  }
  const char* version = wolfSSL_get_version(s.tls);
  if (strcmp(version, "TLSv1.2") && strcmp(version, "TLSv1.3")) return fail("TLS version rejected");
  std::string prefix, suffix;
  if (provider && route == Route::Transcribe) {
    // Boundary includes characters absent from PCM framing but still check payload collisions.
    constexpr const char* boundary = "sticky-provider-wav-7f39c1";
    std::string_view audio(reinterpret_cast<const char*>(body), length);
    if (audio.find(boundary) != audio.npos) return fail("Audio multipart collision; record again");
    prefix = "--sticky-provider-wav-7f39c1\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n"
             "--sticky-provider-wav-7f39c1\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
             "Content-Type: audio/wav\r\n\r\n";
    suffix = "\r\n--sticky-provider-wav-7f39c1--\r\n";
  }
  std::string header = "POST ";
  header += provider ? (route == Route::Chat ? "/v1/chat/completions" : "/v1/audio/transcriptions")
                     : (route == Route::Chat ? "/v1/chat" : "/v1/transcribe");
  header += " HTTP/1.0\r\nHost: " + config.host + "\r\nAuthorization: Bearer " + config.token + "\r\nContent-Type: ";
  header += route == Route::Chat ? "application/json" : provider ? "multipart/form-data; boundary=sticky-provider-wav-7f39c1" : "audio/wav";
  header += "\r\nContent-Length: " + std::to_string(length + prefix.size() + suffix.size()) +
            "\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
  auto write = [&](const uint8_t* bytes, size_t size) {
    size_t sent = 0;
    while (sent < size) {
      if (stopped()) return false;
      n = wolfSSL_write(s.tls, bytes + sent, std::min<size_t>(1024, size - sent));
      if (n > 0)
        sent += n;
      else if (!want(wolfSSL_get_error(s.tls, n)))
        return false;
      else
        delay(5);
    }
    return true;
  };
  if (!write(reinterpret_cast<const uint8_t*>(header.data()), header.size()) ||
      !write(reinterpret_cast<const uint8_t*>(prefix.data()), prefix.size()) || !write(body, length) ||
      !write(reinterpret_cast<const uint8_t*>(suffix.data()), suffix.size()))
    return fail("Request cancelled, timed out or write failed; not retried");
  std::string raw;
  raw.reserve(4096);
  char buffer[192];
  while (!stopped()) {
    n = wolfSSL_read(s.tls, buffer, sizeof(buffer));
    if (n > 0) {
      if (raw.size() + n > kResponseMax + kHeaderMax + 4096) return fail("Backend response exceeds limit");
      raw.append(buffer, n);
      auto split = raw.find("\r\n\r\n");
      if (split == raw.npos && raw.size() > kHeaderMax) return fail("Backend headers exceed limit");
      auto response = provider ? decodeProviderResponse(raw, false) : decodeResponse(raw);
      if (response.ok) {
        if (stopped()) return fail("Cancelled or timed out");
        PostResult r;
        r.ok = true;
        r.body = std::move(response.body);
        r.error = "";
        return r;
      }
    } else {
      int code = wolfSSL_get_error(s.tls, n);
      if (provider && code == WOLFSSL_ERROR_ZERO_RETURN && !stopped()) {
        auto response = decodeProviderResponse(raw, true);
        if (response.ok) return PostResult{true, std::move(response.body), ""};
      }
      if (!want(code)) return fail("Backend rejected request or returned invalid/truncated response");
      delay(5);
    }
  }
  return fail("Cancelled or timed out; request not retried");
}
PostResult post(const OwnerConfig& config, Route route, const uint8_t* body, size_t length,
                const std::atomic<bool>& cancel) {
  return postImpl(config, route, body, length, cancel, false);
}
PostResult postProvider(std::string_view key, Route route, const uint8_t* body, size_t length,
                        const std::atomic<bool>& cancel) {
  OwnerConfig fixed{"https://api.openai.com", kChatRoot, std::string(key), "", false};
  return postImpl(fixed, route, body, length, cancel, true);
}
}  // namespace stickychat
#endif
