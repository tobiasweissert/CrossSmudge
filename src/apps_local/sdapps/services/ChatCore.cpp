#include "ChatCore.h"
namespace stickychat {
namespace {
std::string quote(std::string_view s) {
  std::string out = "\"";
  static const char hex[] = "0123456789abcdef";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c < 32) {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else
      out += c;
  }
  return out + '"';
}
}  // namespace
bool Session::setDraft(std::string text) {
  if (active || text.size() > kDraftMax || text.find('\0') != text.npos) return false;
  draftText = std::move(text);
  return true;
}
uint64_t Session::begin(const std::string& requestId) {
  if (active || draftText.empty() || requestId.empty() || requestId.size() > 64 || history.size() >= 64) return 0;
  std::string body = "{\"request_id\":" + quote(requestId) + ",\"messages\":[";
  for (const auto& m : history) {
    body += "{\"role\":\"";
    body += m.assistant ? "assistant" : "user";
    body += "\",\"content\":" + quote(m.content) + "},";
  }
  body += "{\"role\":\"user\",\"content\":" + quote(draftText) + "}]}";
  if (body.size() > kContextMax) return 0;
  requestBody = std::move(body);
  id = requestId;
  active = ++generation;
  return active;
}
bool Session::complete(uint64_t ticket, std::string_view requestId, std::string reply) {
  if (!active || ticket != active || requestId != id || reply.empty() || reply.size() > kReplyMax ||
      reply.find('\0') != reply.npos)
    return false;
  size_t total = draftText.size() + reply.size();
  for (const auto& m : history) total += m.content.size();
  if (total > kContextMax) return false;
  history.reserve(64);
  history.push_back({false, std::move(draftText)});
  history.push_back({true, std::move(reply)});
  draftText.clear();
  cancel();
  return true;
}
void Session::cancel() {
  ++generation;
  active = 0;
  requestBody.clear();
  id.clear();
}
void Session::clear() {
  cancel();
  history.clear();
  draftText.clear();
}
}  // namespace stickychat
