#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "HttpCore.h"
using namespace ink::http;
int main() {
  std::ifstream fixtures("test/browser/fixtures/redirects.tsv");
  assert(fixtures);
  std::string line;
  while (std::getline(fixtures, line)) {
    const auto tab = line.find('\t');
    assert(tab != line.npos);
    const auto reference = line.substr(0, tab), expected = line.substr(tab + 1);
    Url base{}, result{};
    assert(parseUrl("https://example.org/a/b?keep=1", base));
    assert(resolveRedirect(base, reference, result));
    assert(std::string(result.host) == "example.org" && std::string(result.path) == expected);
    assert(resolveRedirect(base, reference, base));
    assert(std::string(base.host) == "example.org" && std::string(base.path) == expected);
  }
  Url u{};
  assert(parseUrl("https://example.org?at=1230", u) && !strcmp(u.path, "/?at=1230"));
  assert(parseUrl("https://example.org/a/b", u));
  for (auto bad : {"http://example.org/", "mailto:user@example.org", "data:text/html,x", "javascript:alert(1)",
                   "https://user@example.org/", "https://127.0.0.1/", "https://[::1]/", "https://localhost/"}) {
    Url result{};
    assert(!resolveRedirect(u, bad, result));
  }
  assert(!publicV4(0x7f000001) && !publicV4(0x0a000001));
  puts("PASS native/Lua shared URI references, in-place redirect alias, invalid schemes/auth/IP/private guards");
}
