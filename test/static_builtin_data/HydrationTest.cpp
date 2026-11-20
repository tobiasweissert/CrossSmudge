#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "AllocationObserver.h"
#include "StaticBuiltinData.h"

static static_builtin::Context context;
static static_builtin::Backing<char, 6> output;
int main(int argc, char**) {
  if (argc > 1) {
    forbidAllocation = true;
    void* (*volatile allocate)(size_t) = &std::malloc;
    std::printf("unexpected allocation %p\n", allocate(1));
    return 42;
  }
  // Python zlib.compress(b'hello\0', 9), checked by the real vendored decoder.
  static const unsigned char packed[] = {120, 218, 203, 72, 205, 201, 201, 103, 0, 0, 8, 65, 2, 21};
  static_builtin::Block block{packed, sizeof(packed), reinterpret_cast<unsigned char*>(&output),
                              6,      sizeof(output), 3606042942u};
  assert(static_builtin::hydrate(context, &block, 1, nullptr, 0, nullptr, 0));
  assert(context.phase == static_builtin::Phase::Ready);
  assert(std::memcmp(output.values, "hello", 6) == 0);
  assert(output.terminal == 0xa5);
  std::puts("PASS exact_decode");
  EpdGlyph glyphs[1]{};
  EpdUnicodeInterval intervals[] = {{32, 32, 0}};
  EpdFontData font{};
  static const uint8_t bitmap[8]{};
  font.bitmap = bitmap;
  font.glyph = glyphs;
  font.intervals = intervals;
  font.intervalCount = 1;
  static_builtin::FontCheck check{&font, 1, 8};
  glyphs[0].dataLength = 9;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, &check, 1, nullptr, 0));
  assert(context.phase == static_builtin::Phase::Failed);
  glyphs[0].dataLength = 8;
  context.phase = static_builtin::Phase::Empty;
  assert(static_builtin::hydrate(context, nullptr, 0, &check, 1, nullptr, 0));
  intervals[0].offset = 1;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, &check, 1, nullptr, 0));
  std::puts("PASS font_semantic_bounds");
  const char english[] = "hello\0";
  uint16_t offsets[] = {0x8000};
  const uint16_t englishOffsets[] = {0};
  static_builtin::LanguageCheck language{"x", 2, offsets, 1, english, sizeof(english), englishOffsets, false};
  offsets[0] = 0x8001;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, nullptr, 0, &language, 1));
  offsets[0] = 0x8000;
  context.phase = static_builtin::Phase::Empty;
  assert(static_builtin::hydrate(context, nullptr, 0, nullptr, 0, &language, 1));
  offsets[0] = 2;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, nullptr, 0, &language, 1));
  std::puts("PASS language_semantic_bounds");
  static const unsigned char empty[] = {120, 218, 3, 0, 0, 0, 0, 1};
  auto bad = block;
  bad.input = empty;
  bad.inputSize = sizeof(empty);
  bad.decodedSize = 0;
  bad.crc = 0;
  context.phase = static_builtin::Phase::Empty;
  forbidAllocation = true;
  assert(!static_builtin::hydrate(context, &bad, 1, nullptr, 0, nullptr, 0));
  forbidAllocation = false;
  std::puts("PASS reject_zero_payload");
  // Real vendored decoder rejection matrix. No allocating recovery path.
  unsigned char mutated[sizeof(packed) + 1];
  auto reject = [&](const static_builtin::Block& candidate) {
    context.phase = static_builtin::Phase::Empty;
    forbidAllocation = true;
    assert(!static_builtin::hydrate(context, &candidate, 1, nullptr, 0, nullptr, 0));
    assert(context.phase == static_builtin::Phase::Failed);
    assert(!static_builtin::hydrate(context, &block, 1, nullptr, 0, nullptr, 0));
    forbidAllocation = false;
  };
  for (size_t length = 0; length < sizeof(packed); ++length) {
    bad = block;
    bad.inputSize = length;
    reject(bad);
  }
  std::puts("PASS every_truncated_prefix_no_zero_progress");
  std::memcpy(mutated, packed, sizeof(packed));
  mutated[sizeof(packed)] = 0;
  bad = block;
  bad.input = mutated;
  bad.inputSize = sizeof(mutated);
  reject(bad);
  std::puts("PASS trailing_junk_rejected");
  unsigned char concatenated[sizeof(packed) * 2];
  std::memcpy(concatenated, packed, sizeof(packed));
  std::memcpy(concatenated + sizeof(packed), packed, sizeof(packed));
  bad = block;
  bad.input = concatenated;
  bad.inputSize = sizeof(concatenated);
  reject(bad);
  std::memcpy(mutated, packed, sizeof(packed));
  mutated[2] = 7;  // reserved DEFLATE block type, not merely a zlib header error
  bad = block;
  bad.input = mutated;
  reject(bad);
  std::puts("PASS concatenated_stream_and_reserved_deflate_block_rejected");
  for (size_t position : {size_t(0), size_t(1), sizeof(packed) - 1}) {
    std::memcpy(mutated, packed, sizeof(packed));
    mutated[position] ^= 1;
    bad = block;
    bad.input = mutated;
    reject(bad);
  }
  std::puts("PASS malformed_zlib_header_and_adler");
  bad = block;
  bad.crc ^= 1;
  reject(bad);
  std::puts("PASS decoded_crc_rejected");
  bad = block;
  bad.decodedSize = 5;
  reject(bad);  // writes terminal: rejected
  bad = block;
  bad.decodedSize = 4;
  reject(bad);  // can't finish: rejected
  bad = block;
  bad.objectSize = 6;
  reject(bad);  // no in-object terminal capacity
  bad = block;
  bad.decodedSize = 7;
  bad.objectSize = 8;
  reject(bad);  // short result
  std::puts("PASS exact_length_terminal_guard_overrun_short_result");
  // Invalid interval coverage/order/codepoint, glyph overflow and raw groups.
  auto rejectFont = [&]() {
    context.phase = static_builtin::Phase::Empty;
    forbidAllocation = true;
    assert(!static_builtin::hydrate(context, nullptr, 0, &check, 1, nullptr, 0));
    forbidAllocation = false;
  };
  intervals[0] = {33, 32, 0};
  rejectFont();
  intervals[0] = {0x110000, 0x110000, 0};
  rejectFont();
  intervals[0] = {32, 33, 0};
  rejectFont();
  intervals[0] = {32, 32, 0};
  glyphs[0].dataOffset = 0xffffffffu;
  rejectFont();
  glyphs[0].dataOffset = 0;
  EpdFontGroup groups[] = {{0, 9, 8, 1, 0}};
  font.groups = groups;
  font.groupCount = 1;
  rejectFont();
  groups[0].compressedSize = 8;
  groups[0].uncompressedSize = 7;
  rejectFont();
  groups[0].uncompressedSize = 8;
  uint16_t groupIndex[] = {1};
  font.glyphToGroup = groupIndex;
  rejectFont();
  groupIndex[0] = 0;
  context.phase = static_builtin::Phase::Empty;
  assert(static_builtin::hydrate(context, nullptr, 0, &check, 1, nullptr, 0));
  std::puts("PASS malformed_interval_glyph_and_group_bounds");
  offsets[0] = 0x8000;
  language.isEnglish = true;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, nullptr, 0, &language, 1));
  language.isEnglish = false;
  language.data = "xx";
  language.size = 2;
  offsets[0] = 0;
  context.phase = static_builtin::Phase::Empty;
  assert(!static_builtin::hydrate(context, nullptr, 0, nullptr, 0, &language, 1));
  std::puts("PASS english_flag_and_missing_nul_rejected");
}
