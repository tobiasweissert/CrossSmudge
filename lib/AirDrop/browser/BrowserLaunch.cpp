#include "BrowserLaunch.h"

#include <cstring>

#include "../core/Offer.h"

namespace airdrop::browser {
bool LaunchRequest::prepare(std::string_view value) {
  if (state() != LaunchState::Empty) return false;
  char host[128];
  if (!validateUrl(value, host, sizeof host)) {
    problem.store(LaunchError::InvalidUrl);
    return false;
  }
  std::memcpy(url, value.data(), value.size());
  url[value.size()] = 0;
  problem.store(LaunchError::None, std::memory_order_relaxed);
  word.store(static_cast<uint32_t>(LaunchState::Pending), std::memory_order_release);
  return true;
}
LaunchState LaunchRequest::state() const { return static_cast<LaunchState>(word.load(std::memory_order_acquire) & 15); }
uint32_t LaunchRequest::beginAttempt() {
  uint32_t before = word.load(std::memory_order_acquire);
  if ((before & 15) != static_cast<uint32_t>(LaunchState::Pending)) return 0;
  uint32_t token = (before >> 8) + 1;
  if (token > MaxAttempt) return 0;
  uint32_t after = (token << 8) | static_cast<uint32_t>(LaunchState::Offered);
  if (!word.compare_exchange_strong(before, after, std::memory_order_acq_rel)) return 0;
  problem.store(LaunchError::None, std::memory_order_release);
  return token;
}
const char* LaunchRequest::take(uint32_t token) {
  if (!token || token > MaxAttempt) return nullptr;
  uint32_t expected = (token << 8) | static_cast<uint32_t>(LaunchState::Offered);
  return word.compare_exchange_strong(expected, expected | Taken, std::memory_order_acq_rel) ? url : nullptr;
}
bool LaunchRequest::acknowledge(uint32_t token) {
  if (!token || token > MaxAttempt) return false;
  uint32_t expected = (token << 8) | Taken | static_cast<uint32_t>(LaunchState::Offered);
  return word.compare_exchange_strong(expected, (token << 8) | static_cast<uint32_t>(LaunchState::Acknowledged),
                                      std::memory_order_acq_rel);
}
bool LaunchRequest::failAttempt(uint32_t token, LaunchError reason) {
  if (!token || token > MaxAttempt) return false;
  uint32_t before = word.load(std::memory_order_acquire);
  if ((before >> 8) != token || (before & 15) != static_cast<uint32_t>(LaunchState::Offered)) return false;
  if (!word.compare_exchange_strong(before, (token << 8) | static_cast<uint32_t>(LaunchState::Pending),
                                    std::memory_order_acq_rel))
    return false;
  problem.store(reason, std::memory_order_release);
  return true;
}
void LaunchRequest::cancel() {
  uint32_t before = word.load(std::memory_order_acquire);
  for (;;) {
    auto current = static_cast<LaunchState>(before & 15);
    if (current == LaunchState::Acknowledged || current == LaunchState::Cancelled) return;
    if (word.compare_exchange_weak(before, (before & ~uint32_t(31)) | static_cast<uint32_t>(LaunchState::Cancelled),
                                   std::memory_order_acq_rel))
      return;
  }
}
}  // namespace airdrop::browser
