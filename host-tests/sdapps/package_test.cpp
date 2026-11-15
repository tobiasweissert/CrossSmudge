#include "Package.h"
#include <cassert>
#include <cstring>
#include <string>

static constexpr const char* valid =
    "CROSSPLAY-APP 1\nid=browser\ntitle=Browser\ntarget=esp32s3\nabi=1\nimage=app.elf\n"
    "sha256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
int main() {
  sdapps::Package p{};
  assert(sdapps::parsePackage(valid, strlen(valid), "browser", p));
  assert(!strcmp(p.title, "Browser"));
  assert(!strcmp(p.id, "browser"));
  for (size_t n = 0; n < strlen(valid); ++n) {
    assert(!sdapps::parsePackage(valid, n, "browser", p));
  }
  const char* rejected[] = {"../browser", "", "/browser", "browser/", "Browser", "a.b", "a b"};
  for (const auto* name : rejected) assert(!sdapps::parsePackage(valid, strlen(valid), name, p));
  for (const auto* bad : {"abi=3", "target=esp32c3", "image=../app.elf", "title=", "id=other", "sha256=00"}) {
    std::string s(valid);
    auto end = s.find('=', s.find(bad[0]));
    std::string key(bad, strchr(bad, '=') - bad + 1);
    auto at = s.find(key);
    assert(at != std::string::npos);
    end = s.find('\n', at);
    s.replace(at, end-at, bad);
    assert(!sdapps::parsePackage(s.data(), s.size(), "browser", p));
  }
  std::string v2(valid);v2.replace(v2.find("abi=1"),5,"abi=2");
  assert(sdapps::parsePackage(v2.data(),v2.size(),"browser",p) && p.abi==2);
  std::string duplicate = std::string(valid) + "abi=1\n";
  assert(!sdapps::parsePackage(duplicate.data(), duplicate.size(), "browser", p));
  std::string extra = std::string(valid) + "launch=builtin\n";
  assert(!sdapps::parsePackage(extra.data(), extra.size(), "browser", p));
  std::string nul(valid); nul[10] = 0;
  assert(!sdapps::parsePackage(nul.data(), nul.size(), "browser", p));
  std::string huge(1024, 'x');
  assert(!sdapps::parsePackage(huge.data(), huge.size(), "browser", p));
  assert(!sdapps::parsePackage(nullptr, 1, "browser", p));
  assert(!sdapps::parsePackage(valid, strlen(valid), nullptr, p));
  assert(p.id[0] == 0 && p.title[0] == 0);
}
