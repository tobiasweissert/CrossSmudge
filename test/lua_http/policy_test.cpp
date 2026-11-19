#include <cassert>
#include <cstring>
#include <string>

#include "HttpCore.h"
using namespace ink::http;
int main() {
  Url u{};
  assert(parseUrl("https://example.com/a?q=1", u));
  assert(!strcmp(u.host, "example.com") && !strcmp(u.path, "/a?q=1"));
  for (auto s :
       {"http://example.com", "https://127.0.0.1/", "https://2130706433/", "https://[::1]/", "https://a@b.com/",
        "https://a.com:443/", "https://a.com./", "https://a.com/\r\n", "https://a.com/#x", "https://a.com/%0d"})
    assert(!parseUrl(s, u));
  assert(publicV4(0x08080808));
  for (auto ip : {0U, 0x0a000001U, 0x7f000001U, 0xa9fe0001U, 0xac100001U, 0xc0a80001U, 0x64400001U, 0xc0000001U,
                  0xc0000201U, 0xc6336401U, 0xcb007101U, 0xe0000001U, 0xffffffffU})
    assert(!publicV4(ip));
  std::string path;
  assert(resolveAppPath("decks/a.json", "/apps/demo", "/.smudge/network_data/apps/demo", path) &&
         path == "/apps/demo/decks/a.json");
  assert(resolveAppPath("/.smudge/network_data/apps/demo/a", "/apps/demo", "/.smudge/network_data/apps/demo", path));
  for (auto text : {"/apps/demo2/main.lua", "/apps/other/main.lua", "/apps/demo/../secret", "/.smudge/settings.json",
                    "/.smudge/saved_data/demo/a"})
    assert(!resolveAppPath(text, "/apps/demo", "/.smudge/network_data/apps/demo", path));
  assert(safeRelativePath("decks/a.json"));
  for (auto s : {"../key", "/key", "a/../key", "a//key", "a\\key", ".hidden", "a/.hidden", "a/", ""})
    assert(!safeRelativePath(s));
  assert(safeHeader("Accept", "application/json"));
  assert(safeHeader("X-Request-Id", "abc"));
  for (auto s :
       {"Authorization", "Cookie", "Host", "Connection", "Content-Length", "Transfer-Encoding", "Proxy-Authorization"})
    assert(!safeHeader(s, "x"));
  assert(!safeHeader("Accept", "a\r\nb"));
}
