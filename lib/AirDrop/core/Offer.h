#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace airdrop {
constexpr size_t MaxAskBytes = 524288;
constexpr uint32_t MaxFileBytes = 30000000;
enum class OfferKind : uint8_t { None, Url, File };
enum class ParseError : uint8_t { None, Bounds, Graph, Unsupported, Metadata, Url, Filename, Size };
// Owned value; no pointers into the Ask buffer survive parsing. Sender is unverified.
struct Offer {
  OfferKind kind = OfferKind::None;
  char sender[129]{};
  char url[513]{};
  char host[128]{};
  char filename[121]{};
  char archivePath[244]{};
  char fileType[65]{};
  char transferId[37]{};
  uint32_t fileSize = 0;
};
// Caller owns/reuses this fixed graph workspace for the receive-session lifetime.
// Keeping it outside parser stack avoids a 4 KiB listener stack frame; no allocations.
struct OfferScratch {
  uint8_t colors[4096]{};
  struct Frame {
    uint16_t id = 0, child = 0;
    uint8_t height = 0;
    bool entered = false;
  };
  Frame frames[17]{}; // Explicit graph stack keeps hostile nesting off the task stack.
  uint32_t visits = 0;
  uint32_t decodedStringBytes = 0;
};
bool validateUrl(std::string_view url, char* host, size_t hostCapacity);
bool safeFilename(std::string_view name);
bool parseOffer(const uint8_t* body, size_t size, Offer& out, OfferScratch& scratch, ParseError& error);
}  // namespace airdrop
