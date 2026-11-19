#!/usr/bin/env python3
"""Compile real portable utility bodies; no SDK, downloads or full build."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROBE = r'''
#include "Utf8.h"
#include "Memory.h"
#include "BuildScratch.h"
#include <cassert>
#include <cstring>
#include <vector>
struct Lifetime { static inline int count = 0; Lifetime(){++count;} ~Lifetime(){--count;} };
int main() {
  // Upstream NFC input-anchor reporting and dictionary decomposition stay intact.
  std::vector<uint32_t> absorbed;
  auto callback = [](uint32_t p, void* ctx) { static_cast<std::vector<uint32_t>*>(ctx)->push_back(p); return true; };
  assert(utf8ComposeNfc("e\xcc\x81", callback, &absorbed) == "\xc3\xa9");
  assert(absorbed == std::vector<uint32_t>{1});
  assert(utf8ComposeNfc("e\xcc\x81", [](uint32_t, void*){return false;}) == "");
  assert(utf8DecomposedBase(0xe9) == 'e');
  assert(utf8SmallCapsUpper(0x3c2) == 0x3a3);
  assert(utf8IsHangul(0xac00));
  assert(isHebrewMark(0x05c9));
  assert(isArabicVowelMark(0x064e));
  // Owner lookup cleanup, fallback-script admission and variation selectors.
  assert(utf8CleanLookupWord("\xe2\x80\x9c" "cafe\xcc\x81" "!\xe2\x80\x9d") == "caf\xc3\xa9");
  assert(!utf8ContainsLookupCharacter("\xe2\x80\x94!"));
  assert(!utf8ContainsLookupCharacter("\xcc\x81"));
  assert(utf8ContainsLookupCharacter("\xe6\x97\xa5"));
  assert(utf8IsCjkCodepoint(0x30000));
  assert(utf8IsCjkWordCharacter(0x65e5));
  assert(!utf8IsCjkWordCharacter(0x3002));
  assert(utf8IsVariationSelector(0xe0100));
  std::string word = "a\xf0\x9f\x93\x96";
  utf8TruncateChars(word, 1); assert(word == "a");
  const unsigned char malformed[] = {0xe2, 0x82, 'x', 0};
  const auto* cursor = malformed;
  assert(utf8NextCodepoint(&cursor) == REPLACEMENT_GLYPH && cursor == malformed + 2);
  char buffer[] = "e\xcc\x81";
  utf8ComposeNfcInPlace(buffer); assert(std::strcmp(buffer, "\xc3\xa9") == 0);
  // Upstream vector compaction coexists with owner's explicit heap ownership.
  std::vector<int> values; values.reserve(100); values.push_back(7);
  shrinkToFit(values); assert(values.size() == 1 && values[0] == 7 && values.capacity() == 1);
  auto bytes = makeInternalByteBufferNoThrow(128); assert(bytes);
  assert(byteBufferPool(bytes.get()) == MemoryPool::Internal);
  assert(!makePsramByteBufferNoThrow(128));
  assert(!makeAlignedByteBufferNoThrow(SIZE_MAX));
  { HeapObject<Lifetime> object; assert(object.init(MemoryPool::Internal)); assert(Lifetime::count == 1); assert(!object.init(MemoryPool::Internal)); }
  assert(Lifetime::count == 0);
  alignas(16) uint8_t scratch[256];
  buildscratch::lend(scratch, sizeof scratch);
  assert(buildscratch::available(256));
  assert(buildscratch::claim(256) == scratch);
  assert(!buildscratch::available(1)); assert(!buildscratch::claim(1));
  buildscratch::release(scratch); assert(buildscratch::available(256));
  buildscratch::reclaim(); assert(!buildscratch::available(1));
}
'''
with tempfile.TemporaryDirectory(prefix='portable-foundation-', dir=os.environ.get('TMPDIR')) as tmp:
    tmp = Path(tmp)
    (tmp / 'probe.cpp').write_text(PROBE)
    (tmp / 'Logging.h').write_text('#pragma once\n#define LOG_ERR(...) ((void)0)\n')
    command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti',
               '-I' + str(tmp), '-I' + str(ROOT / 'lib/Utf8'), '-I' + str(ROOT / 'lib/Memory'),
               str(tmp / 'probe.cpp'), str(ROOT / 'lib/Utf8/Utf8.cpp'), str(ROOT / 'lib/Memory/BuildScratch.cpp'),
               '-o', str(tmp / 'probe')]
    subprocess.run(command, check=True, timeout=30)
    subprocess.run([str(tmp / 'probe')], check=True, timeout=10)
print('portable foundation: real Utf8, Memory and BuildScratch contracts passed')
