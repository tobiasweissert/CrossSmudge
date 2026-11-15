#pragma once
#include <atomic>

#include "ChatProtocol.h"
namespace stickychat {
enum class Route { Chat, Transcribe };
struct PostResult {
  bool ok = false;
  std::string body;
  const char* error = "";
};
PostResult postProvider(std::string_view key, Route route, const uint8_t* body, size_t length,
                        const std::atomic<bool>& cancel);
PostResult post(const OwnerConfig& config, Route route, const uint8_t* body, size_t length,
                const std::atomic<bool>& cancel);
}  // namespace stickychat
