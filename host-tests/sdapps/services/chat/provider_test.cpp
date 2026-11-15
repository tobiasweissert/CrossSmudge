#include "ChatProtocol.h"
#include "ChatCore.h"
#include <cassert>
#include <cstdio>
using namespace stickychat;
int main() {
  assert(validProviderKey("sk-test_canary-123"));
  assert(!validProviderKey("sk-test\r\nHost: evil"));
  assert(!validProviderKey(std::string(513, 'a')));
  Session s; s.setDraft("hello"); assert(s.begin("local"));
  auto body = providerChatBody(s.request());
  assert(body.find("request_id") == body.npos);
  assert(body.find("gpt-4.1-mini") != body.npos);
  assert(body.find("\"store\":false") != body.npos);
  auto good = R"({"id":"provider-id","choices":[{"index":0,"message":{"role":"assistant","content":"Hello\nworld"},"finish_reason":"stop"}],"usage":{"total_tokens":5}})";
  auto e = parseProviderEnvelope(good, false);
  assert(e.ok && e.text == "Hello\nworld");
  assert(!parseProviderEnvelope(R"({"choices":[{"message":{"role":"assistant","content":"partial"},"finish_reason":"length"}]})", false).ok);
  assert(!parseProviderEnvelope(R"({"choices":[{"message":{"role":"assistant","content":"\u0000"},"finish_reason":"stop"}]})", false).ok);
  assert(!parseProviderEnvelope(R"({"choices":[{"message":{"role":"user","content":"bad"},"finish_reason":"stop"}]})", false).ok);
  assert(parseProviderEnvelope(R"({"text":"review me","usage":{"seconds":2}})", true).ok);
  assert(!parseProviderEnvelope(R"({"text":"a","text":"b"})", true).ok);
  assert(!parseProviderEnvelope("{\"text\":\"" + std::string(kDraftMax+1,'x') + "\"}", true).ok);
  const std::string chunks="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n";
  assert(decodeProviderResponse(chunks,false).ok);
  assert(!decodeProviderResponse(chunks.substr(0,chunks.size()-2),true).ok);
  const std::string close="HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n{}";
  assert(!decodeProviderResponse(close,false).ok);
  assert(decodeProviderResponse(close,true).ok);
  assert(!decodeProviderResponse("HTTP/1.1 302 Found\r\nContent-Type: application/json\r\n\r\n{}",true).ok);
  puts("provider request/response bounds: PASS");
}
