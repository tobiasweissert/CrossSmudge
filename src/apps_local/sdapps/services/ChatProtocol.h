#pragma once
#include "ChatCore.h"
namespace stickychat {
struct OwnerConfig {
  std::string url, ca, token, host;
  bool allowPrivate = false;
};
struct Envelope;
bool validProviderKey(std::string_view key);
std::string providerChatBody(std::string_view sessionRequest);
Envelope parseProviderEnvelope(std::string_view json, bool transcription);
bool validateConfig(OwnerConfig& config);
bool allowedIpv4(uint32_t address, bool allowPrivate);
struct Envelope {
  bool ok = false;
  std::string id, text;
};
Envelope parseEnvelope(std::string_view json, bool transcription);
// Strict, bounded HTTP response: 200 JSON, one Content-Length; no redirects/chunks/encoding.
struct Response {
  bool ok = false;
  std::string body;
};
Response decodeProviderResponse(std::string_view raw, bool cleanClose);
Response decodeResponse(std::string_view raw);
inline constexpr size_t kResponseMax = 6 * kReplyMax + 512, kHeaderMax = 4096;
}  // namespace stickychat
