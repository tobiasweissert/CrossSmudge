#include <cassert>
#include <cstdio>

#include "util/StringUtils.h"
#include "util/UrlUtils.h"
int main() {
  assert(StringUtils::sanitizeFilename(" ../chapter: one.epub ") == "_chapter_ one.epub");
  auto name = StringUtils::sanitizeFilenamePreservingExtension("漢字日本語漢字日本語.epub", 17);
  assert(name.size() <= 17 && name.ends_with(".epub"));
  // Full codepoint ends before extension, no chopped UTF-8 continuation byte.
  assert((name.size() - 5) % 3 == 0);
  assert(StringUtils::asciiCaseCmp("Book", "book") == 0);
  std::string path = "/opds/books";
  assert(UrlUtils::resolveServerPath(path, "https://example.org:8443/catalog") &&
         path == "https://example.org:8443/opds/books");
  path = "//foreign.invalid/x";
  assert(!UrlUtils::resolveServerPath(path, "https://example.org/catalog"));
  assert(UrlUtils::isHttpsUrl("https://example.org"));
  assert(!UrlUtils::isForeignHost("crosscub.local:80") && !UrlUtils::isForeignHost("192.168.1.1"));
  assert(UrlUtils::isForeignHost("example.org:80"));
  puts("PASS UTF8 filename bounds/extensions, ASCII comparison, modern server-relative URL and hotspot host API");
}
