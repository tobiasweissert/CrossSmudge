#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace stickychat {
inline constexpr size_t kContextMax = 32768, kReplyMax = 16384, kDraftMax = 8192, kPcmMax = 960000,
                        kWavMax = kPcmMax + 44;
struct Message {
  bool assistant;
  std::string content;
};
class Session {
 public:
  bool setDraft(std::string text);
  uint64_t begin(const std::string& requestId);
  bool complete(uint64_t ticket, std::string_view requestId, std::string reply);
  void cancel();
  void clear();
  const std::string& draft() const { return draftText; }
  const std::string& request() const { return requestBody; }
  const std::vector<Message>& messages() const { return history; }

 private:
  uint64_t generation = 0, active = 0;
  std::string draftText, requestBody, id;
  std::vector<Message> history;
};
}  // namespace stickychat
