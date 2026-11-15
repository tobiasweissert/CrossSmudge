#include <cassert>
#include <cstdio>

#include "ChatProtocol.h"
using namespace stickychat;
int main() {
  OwnerConfig c{"https://chat.owner.example", "-----BEGIN CERTIFICATE-----\nownerCA", "device-canary", "", false};
  assert(validateConfig(c) && c.host == "chat.owner.example");
  for (auto url : {"http://chat.owner.example", "https://chat.owner.example/evil", "https://user@host",
                   "https://127.0.0.1", "https://host:8443", "https://a..b", "https://host\r\nx:1"}) {
    auto bad = c;
    bad.url = url;
    assert(!validateConfig(bad));
  }
  // lwIP accepts inet_aton-style whole/short/dotted hex, decimal and octal.
  // This is configuration admission, not an address-policy/SSRF bypass claim.
  for (auto host : {"0x7f000001", "0X7F000001", "0x08080808", "0x7f.1", "127.0x1",
                    "0x7f.0.0.1", "0x7f.0x0.0x0.0x1", "0x", "0x.1", "0177.0.0.1", "017700000001",
                    "127.1", "127.0.1", "2130706433", "8.8.8.8", "010.010.010.010"}) {
    auto numeric = c;
    numeric.url = std::string("https://") + host;
    assert(!validateConfig(numeric) && numeric.host.empty());
  }
  for (auto host : {"0xchat.example", "0x7f.example", "dead.beef", "123.example", "chat-01.example",
                    "0x1.0x2.0x3.0x4.example", "0xg"}) {
    auto dns = c;
    dns.url = std::string("https://") + host;
    assert(validateConfig(dns) && dns.host == host);
  }
  auto bad = c;
  bad.token = "x\r\nInjected: 1";
  assert(!validateConfig(bad));
  assert(allowedIpv4(0x08080808, false));
  assert(allowedIpv4(0xc0020101, false));
  assert(!allowedIpv4(0xc0000201, false));
  assert(!allowedIpv4(0xc0586301, false));
  assert(!allowedIpv4(0x7f000001, true));
  assert(!allowedIpv4(0x0a000001, false));
  assert(allowedIpv4(0x0a000001, true));
  assert(!allowedIpv4(0xa9fe0001, true));
  assert(!allowedIpv4(0x64400001, false));
  auto e = parseEnvelope("{\"request_id\":\"a\",\"reply\":\"<b>plain</b> \\ud83d\\ude00\"}", false);
  assert(e.ok && e.id == "a" && e.text == "<b>plain</b> 😀");
  assert(parseEnvelope("{\"text\":\"edit me\"}", true).ok);
  for (auto j : {"{}", "{\"reply\":\"x\",\"request_id\":\"a\",\"reply\":\"y\"}", "{\"text\":17}",
                 "{\"text\":\"\\ud800\"}", "{\"text\":\"\\u0000\"}", "{\"text\":\"x\"} trailing"})
    assert(!parseEnvelope(j, true).ok);
  assert(!parseEnvelope("{\"request_id\":\"a\",\"reply\":\"x\\u0000y\"}", false).ok);
  assert(!parseEnvelope(std::string("{\"text\":\"x") + char(0) + "y\"}", true).ok);
  assert(parseEnvelope("{\"text\":\"" + std::string(kDraftMax, 'a') + "\"}", true).ok);
  assert(!parseEnvelope("{\"text\":\"" + std::string(kDraftMax + 1, 'a') + "\"}", true).ok);
  auto response = [](std::string h, std::string b) {
    return decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n" + h + "\r\n" + b);
  };
  assert(response("Content-Length: 2\r\n", "{}").ok);
  for (auto h :
       {"", "Content-Length: 3\r\n", "Content-Length: 2\r\nContent-Length: 2\r\n",
        "Content-Length: 2\r\nTransfer-Encoding: chunked\r\n", "Content-Length: 2\r\nContent-Encoding: gzip\r\n"})
    assert(!response(h, "{}").ok);
  assert(!decodeResponse("HTTP/1.1 302 Found\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}").ok);
  std::puts("fixed-origin configuration/JSON/HTTP bounds: PASS");
}
