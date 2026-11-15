#include <cassert>
#include <cstdio>

#include "ChatPost.h"
#include "PlatformFakes.h"
using namespace stickychat;
int main() {
  OwnerConfig config{"https://chat.owner.example", "-----BEGIN CERTIFICATE-----\nfixture", "canary-token", "", false};
  std::atomic<bool> cancel{false};
  auto run = [&] {
    auto r = post(config, Route::Chat, reinterpret_cast<const uint8_t*>("{}"), 2, cancel);
    assert(!fake::tcpOpen && !fake::contexts && !fake::sessions);
    return r;
  };
  fake::reset();
  assert(run().ok);
  assert(fake::minimum && fake::verify && fake::loaded && fake::sni && fake::identity);
  assert(fake::sent.find("POST /v1/chat HTTP/1.0\r\nHost: chat.owner.example\r\n") == 0);
  assert(fake::sent.find("Authorization: Bearer canary-token\r\n") != std::string::npos);
  fake::reset();
  fake::trusted = false;
  assert(!run().ok && fake::sent.empty());
  fake::reset();
  fake::clockOffset = 3600;
  assert(!run().ok && fake::sent.empty());
  fake::reset();
  fake::chain = false;
  assert(!run().ok && fake::sent.empty());
  fake::reset();
  fake::host = false;
  assert(!run().ok && fake::sent.empty());
  fake::reset();
  fake::address = 0x7f000001;
  config.allowPrivate = true;
  assert(!run().ok && fake::sent.empty());
  fake::reset();
  fake::address = 0x0a000001;
  config.allowPrivate = false;
  assert(!run().ok);
  fake::reset();
  fake::address = 0x0a000001;
  config.allowPrivate = true;
  assert(run().ok);
  fake::reset();
  cancel = true;
  assert(!run().ok && fake::sent.empty());
  cancel = false;
  fake::reset();
  fake::response = "HTTP/1.1 302 Found\r\nLocation: https://evil.example/\r\nContent-Length: 0\r\n\r\n";
  assert(!run().ok);
  fake::reset();
  std::string oversized(kContextMax + 1, 'a');
  assert(!post(config, Route::Chat, reinterpret_cast<const uint8_t*>(oversized.data()), oversized.size(), cancel).ok &&
         fake::sent.empty());
  fake::reset(); fake::expectedHost="api.openai.com";
  auto direct = [&] { return postProvider("sk-provider-canary", Route::Chat,
    reinterpret_cast<const uint8_t*>("{}"),2,cancel); };
  assert(direct().ok);
  assert(fake::sni && fake::identity && fake::minimum && fake::verify);
  assert(fake::sent.find("POST /v1/chat/completions HTTP/1.0\r\nHost: api.openai.com\r\n") == 0);
  fake::reset(); fake::address=0x0a000001;
  assert(!direct().ok && fake::sent.empty());
  fake::reset(); fake::expectedHost="api.openai.com";
  uint8_t wav[46]{};
  assert(postProvider("sk-provider-canary",Route::Transcribe,wav,sizeof(wav),cancel).ok);
  assert(fake::sent.find("/v1/audio/transcriptions") != std::string::npos);
  assert(fake::sent.find("multipart/form-data") != std::string::npos);
  assert(fake::sent.find("whisper-1") != std::string::npos);
  std::puts("production POST adapter admission/configuration/cleanup (fake DNS/TLS, NOT crypto qualification): PASS");
}
