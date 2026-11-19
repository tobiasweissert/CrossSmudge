#include <cassert>
#include <cstdio>
#include <string>

#include "credential.inc"
#include "network/ProtectedPaths.h"
int main() {
  const char* families[] = {"wifi.json", "opds.json", "koreader.json", "opds_tokens.json", "lcp_passphrases.json"};
  size_t cases = 0;
  for (auto family : families)
    for (auto suffix : {"", ".tmp", ".bak", ".old", ".new"}) {
      const std::string base = "/.crosspoint/" + std::string(family) + suffix;
      for (auto path : {base, "//.crosspoint/./" + std::string(family) + suffix,
                        "/safe/../.crosspoint/" + std::string(family) + suffix,
                        "/ .crosspoint. / " + std::string(family) + suffix + ". "}) {
        assert(protectedpaths::isSensitivePath(path.c_str()));
        assert(networkCredentialPath(path.c_str()));
        assert(!protectedpaths::isPluginPath(path));
        ++cases;
        for (char& c : path)
          if (c >= 'a' && c <= 'z') c -= 32;
        assert(protectedpaths::isSensitivePath(path.c_str()));
        assert(networkCredentialPath(path.c_str()));
        ++cases;
      }
    }
  for (auto p : {"/CROSSP~1/OPDS_T~1.JSO", "/.crosspoint/LCP_PA~1.JSO", "/.crosspoint/ OPDS_T~1.JSO. ",
                 "/.crosspoint\\opds_tokens.json", "\\.crosspoint\\lcp_passphrases.json"}) {
    assert(protectedpaths::isSensitivePath(p));
    assert(networkCredentialPath(p));
    ++cases;
  }
  for (auto p :
       {"/books/book.epub", "/.crosspoint/settings.json", "/notes/opds_tokens.json", "/books/volume..2.epub"}) {
    assert(!protectedpaths::isSensitivePath(p));
    assert(!networkCredentialPath(p));
    ++cases;
  }
  assert(protectedpaths::isSensitivePath(nullptr));
  assert(protectedpaths::isPluginPath("/books/book.epub"));
  std::printf("global + identical frozen web/WebDAV wrapper credential policy: %zu cases PASS\n", cases);
}
