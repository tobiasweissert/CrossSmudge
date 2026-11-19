#!/usr/bin/env python3
"""Dependency-free production EpdFont/Family API and synthetic metric tests.
Not SDK rasterization, real font pixels or device acceptance.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]

class FontCompatibility(unittest.TestCase):
    def test_font_id_generator_preserves_owner_and_upstream_names(self):
        import re
        generated = subprocess.check_output(['bash', str(ROOT/'lib/EpdFont/scripts/build-font-ids.sh')], text=True)
        ids = dict(re.findall(r'#define (\w+) \((-?\d+)\)', generated))
        checked = dict(re.findall(r'#define (\w+) \((-?\d+)\)', (ROOT/'src/fontIds.h').read_text()))
        self.assertEqual(ids, checked)
        for name in ['LEXENDDECA_10_FONT_ID','BITTER_16_FONT_ID','NOTOSANS_18_FONT_ID','UI_10_FONT_ID']:
            self.assertIn(name, ids)
        self.assertEqual(ids['UI_10_FONT_ID'], '-1313900173')
        self.assertEqual(ids['UI_12_FONT_ID'], '1544960787')
        self.assertEqual(ids['SMALL_FONT_ID'], '1406075677')
        self.assertEqual(len(set(ids.values())), len(ids))

    def test_builtin_headers_parse_with_extended_font_data(self):
        with tempfile.TemporaryDirectory(prefix='font-assets-', dir=os.environ.get('TMPDIR')) as tmp:
            p=Path(tmp)/'assets.cpp'
            p.write_text('#include "EpdFontData.h"\n#include "builtinFonts/all.h"\n#include "fontIds.h"\n')
            subprocess.run(['g++','-std=c++20','-fsyntax-only','-Wall','-Wextra','-Werror',
                            '-Wno-missing-field-initializers','-I'+str(ROOT/'lib/EpdFont'),
                            '-I'+str(ROOT/'src'),str(p)],check=True)

    def test_production_family_geometry_and_vector_callbacks(self):
        source = r'''
#include "EpdFontFamily.h"
#include <cassert>
#include <cmath>
static int8_t kern(void*, uint32_t a, uint32_t b) { return a == 'A' && b == 'V' ? -8 : 0; }
static const EpdGlyph vectorGlyph = {7, 9, 128, 0, 9, 0, 0};
static const EpdGlyph* miss(void*, uint32_t cp) { return cp == 0x4E00 ? &vectorGlyph : nullptr; }
static bool covers(void*, uint32_t cp) { return cp == 0x4E00; }
static const uint8_t* bitmap(void*, const EpdGlyph*) { static uint8_t b=0xFF; return &b; }
int main() {
  static_assert(EpdFontFamily::RUBY_CONTINUE == 64);
  static_assert(EpdFontFamily::SMALL_CAPS == 128);
  static_assert(FONT_RASTER_DPI == 150);
  const EpdGlyph glyphs[] = {{4, 6, 96, 0, 6, 0, 0}, {5, 8, 112, 0, 8, 0, 0}};
  const EpdUnicodeInterval intervals[] = {{65,65,0},{86,86,1}};
  EpdFontData data{};
  data.glyph = glyphs; data.intervals = intervals; data.intervalCount = 2;
  data.ascender = 9; data.descender = -2; data.advanceY = 12;
  data.kernHandler = kern;
  EpdFont primary(&data);
  EpdFontData fallbackData{};
  const EpdGlyph fallbackGlyph[] = {{9, 11, 160, 0, 11, 0, 0}};
  const EpdUnicodeInterval fallbackInterval[] = {{0x3A9,0x3A9,0}};
  fallbackData.glyph = fallbackGlyph; fallbackData.intervals = fallbackInterval;
  fallbackData.intervalCount = 1;
  EpdFont fallback(&fallbackData);
  EpdFontFamily sized(12, &primary, nullptr, nullptr, nullptr, &fallback);
  EpdFontFamily legacy(&primary, nullptr, nullptr, nullptr, &fallback);
  assert(std::fabs(sized.getSizeInPixels()-25.0f)<0.01f);
  assert(legacy.getSizeInPixels()>0);
  assert(sized.hasCodepoint(0x3A9));
  auto found = sized.getGlyphData(0x3A9);
  assert(found.fontData == &fallbackData && found.glyph == fallbackGlyph);
  assert(sized.getGlyph(0x3A9) == fallbackGlyph);
  assert(primary.getKerning('A','V') == -8);
  EpdFontData vectorData{};
  vectorData.glyphMissHandler=miss; vectorData.coverageHandler=covers;
  vectorData.vectorBitmapHandler=bitmap; vectorData.kernHandler=kern;
  EpdFont runtime(&vectorData); EpdFontFamily runtimeFamily(14,&runtime);
  assert(runtime.hasCodepoint(0x4E00));
  assert(runtime.findGlyph(0x4E00)==&vectorGlyph);
  assert(runtime.findGlyph(0x4E01)==nullptr);
  auto dynamic = runtimeFamily.getGlyphData(0x4E00);
  assert(dynamic.fontData==&vectorData && dynamic.glyph==&vectorGlyph);
  assert(*dynamic.fontData->vectorBitmapHandler(nullptr,dynamic.glyph)==0xFF);
  assert(runtimeFamily.getKerning('A','V')==-8);
  EpdGlyph solid{};
  assert(sized.getGlyphMetrics(0x2588, solid) == &solid);
  assert(solid.width > 0 && solid.advanceX > 0);
  int w=0,h=0;
  sized.getTextDimensions("AV",&w,&h);
  assert(w == 11 && h == 8);
  sized.getTextDimensions("\xCE\xA9",&w,&h);
  assert(w == 9 && h == 11);
  sized.getTextDimensions("",&w,&h);
  assert(w == 0 && h == 0);
  assert(combiningMark::anchorOver(combiningMark::Anchor::RightNative,10,1,8,0,2)==17);
  assert(fp4::toPixel(96-8)==6);
}
'''
        with tempfile.TemporaryDirectory(prefix='fonts-', dir=os.environ.get('TMPDIR')) as tmp:
            p=Path(tmp); (p/'probe.cpp').write_text(source)
            cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',
                 '-I'+str(ROOT/'lib/EpdFont'),'-I'+str(ROOT/'lib/Utf8'),
                 str(p/'probe.cpp'),str(ROOT/'lib/EpdFont/EpdFont.cpp'),
                 str(ROOT/'lib/EpdFont/EpdFontFamily.cpp'),str(ROOT/'lib/Utf8/Utf8.cpp'),'-o',str(p/'probe')]
            if os.environ.get('FONT_SANITIZE'):
                cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(cmd, check=True)
            subprocess.run([str(p/'probe')], check=True)

if __name__ == '__main__':
    unittest.main()
