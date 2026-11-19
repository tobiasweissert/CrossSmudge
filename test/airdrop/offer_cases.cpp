#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Consent.h"
#include "HttpRequest.h"
#include "Offer.h"

using namespace airdrop;
int main(int argc, char** argv) {
  assert(argc == 2);
  assert(safeFilename("ordinary.epub") && safeFilename("book.txt"));
  assert(!safeFilename("book .txt") && !safeFilename("book..epub") && !safeFilename("CON.txt"));
  for (const char* name :
       {"x\xe2\x80\x8e.txt", "x\xe2\x80\x8f.txt", "x\xe2\x80\xa8.txt", "x\xe2\x80\xa9.txt", "x\xef\xbb\xbf.txt"})
    assert(!safeFilename(name));
  static OfferScratch scratch;
  static Offer offer;
  const char* good[] = {"dynamic_type", "no_type",       "pdf",       "image",      "archive",  "binary",
                        "font",         "extensionless", "script",    "url",        "url_noid", "url_utf16_sender",
                        "file",         "file_limit",    "file_noid", "large_icon", "ask_limit"};
  const char* bad[] = {
      "ask_over",    "file_wrapper",      "mixed",           "batch",         "empty",       "http",
      "bare",        "userinfo",          "private_literal", "encoded_slash", "control_url", "long_url",
      "file_over",   "traversal",         "unsafe_fat",      "device_name",   "directory",   "bad_size",
      "bad_utf16",   "bad_offset",        "cycle",           "duplicate_key", "bad_ref",     "bad_type",
      "bad_trailer", "huge_object_count", "too_deep",        "bad_bom"};
  auto check = [&](const char* name, bool expected) {
    std::ifstream f(std::string(argv[1]) + "/" + name, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    ParseError err{};
    bool ok = parseOffer(b.data(), b.size(), offer, scratch, err);
    if (ok != expected) {
      std::fprintf(stderr, "%s: expected %d got %d err=%u\n", name, expected, ok, unsigned(err));
      return false;
    }
    if (ok && offer.kind == OfferKind::Url) {
      assert(std::strcmp(offer.url, "https://example.com/a#fragment") == 0);
      assert(std::strcmp(offer.host, "example.com") == 0);
    }
    return true;
  };
  for (const char* name : good) assert(check(name, true));
  for (const char* name : bad) assert(check(name, false));
  // Adversarial truncation and mutation exercise the same production graph reader.
  // Acceptance may vary with the mutation; sanitizer findings must never occur.
  std::ifstream seed(std::string(argv[1]) + "/url", std::ios::binary);
  std::vector<uint8_t> original((std::istreambuf_iterator<char>(seed)), {});
  ParseError mutationError{};
  for (size_t cut = 0; cut < original.size(); ++cut) {
    assert(!parseOffer(original.data(), cut, offer, scratch, mutationError));
  }
  for (size_t i = 0; i < original.size(); ++i) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      auto changed = original;
      changed[i] ^= static_cast<uint8_t>(1u << bit);
      (void)parseOffer(changed.data(), changed.size(), offer, scratch, mutationError);
    }
  }
  assert(sizeof(OfferScratch) <= 4608);
  assert(validateUrl("https://example.com/a#fragment", offer.host, sizeof offer.host));
  assert(!validateUrl(" https://example.com", offer.host, sizeof offer.host));
  assert(!validateUrl("https://example.com/%5c", offer.host, sizeof offer.host));
  assert(!validateUrl("https://example.com/%00", offer.host, sizeof offer.host));
  assert(!validateUrl("https://example.com:443/", offer.host, sizeof offer.host));
  assert(!validateUrl("https://example.com/%x0", offer.host, sizeof offer.host));
  std::puts("offer production cases passed");
}
