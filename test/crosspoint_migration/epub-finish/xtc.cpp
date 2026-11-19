#include "Xtc.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "Bitmap.h"
using namespace xtc;
static std::vector<uint8_t> book(uint32_t container, uint32_t magic, uint16_t w, uint16_t h) {
  XtcHeader hd{};
  hd.magic = container;
  hd.versionMajor = 1;
  hd.pageCount = 1;
  hd.pageTableOffset = sizeof(hd);
  hd.dataOffset = sizeof(hd) + sizeof(PageTableEntry);
  XtgPageHeader p{};
  p.magic = magic;
  p.width = w;
  p.height = h;
  p.dataSize = magic == XTH_MAGIC ? size_t(w) * ((h + 7) / 8) * 2 : ((w + 7) / 8) * h;
  PageTableEntry e{hd.dataOffset, uint32_t(sizeof(p) + p.dataSize), w, h};
  std::vector<uint8_t> b(hd.dataOffset + e.dataSize, 0x55);
  std::memcpy(b.data(), &hd, sizeof(hd));
  std::memcpy(b.data() + hd.pageTableOffset, &e, sizeof(e));
  std::memcpy(b.data() + hd.dataOffset, &p, sizeof(p));
  return b;
}
int main() {
  for (auto container : {XTC_MAGIC, XTCH_MAGIC})
    for (auto magic : {XTG_MAGIC, XTH_MAGIC})
      for (auto h : {uint16_t(8), uint16_t(9)}) {
        Storage.reset();
        auto b = book(container, magic, 8, h);
        Storage.put("/test.xtc", b);
        XtcParser p;
        assert(p.open("/test.xtc") == XtcError::OK);
        assert(p.getBitDepth() == (magic == XTH_MAGIC ? 2 : 1));
        PageInfo i{};
        assert(p.getPageInfo(0, i));
        const size_t count = magic == XTH_MAGIC ? 8 * ((h + 7) / 8) * 2 : h;
        std::vector<uint8_t> out(count);
        assert(p.loadPage(0, out.data(), out.size()) == count);
        for (auto x : out) assert(x == 0x55);
        size_t n = 0;
        assert(p.loadPageStreaming(
                   0,
                   [&](const uint8_t* data, size_t size, size_t off) {
                     assert(n == off);
                     n += size;
                     for (size_t j = 0; j < size; ++j) assert(data[j] == 0x55);
                   },
                   3) == XtcError::OK);
        assert(n == count);
        assert(p.loadPageStreaming(0, [](const uint8_t*, size_t, size_t) {}, 0) == XtcError::MEMORY_ERROR);
        auto hd = reinterpret_cast<const XtcHeader*>(b.data());
        auto pg = reinterpret_cast<XtgPageHeader*>(b.data() + hd->dataOffset);
        pg->dataSize++;
        Storage.put("/bad.xtc", b);
        XtcParser bad;
        assert(bad.open("/bad.xtc") == XtcError::OK);
        n = 0;
        assert(bad.loadPageStreaming(
                   0, [&](const uint8_t*, size_t size, size_t) { n += size; }, 3) == XtcError::CORRUPTED_HEADER);
        assert(n == 0);
        Xtc x("/test.xtc", "/cache");
        assert(x.load());
        assert(x.generateCoverBmp());
        HalFile cover;
        assert(Storage.openFileForRead("test", x.getCoverBmpPath(), cover));
        Bitmap bmp(cover);
        assert(bmp.parseHeaders() == BmpReaderError::Ok);
        assert(bmp.getWidth() == 8);
        assert(bmp.getHeight() == h);
        assert(bmp.getBpp() == (magic == XTH_MAGIC ? 2 : 1));
        assert(x.generateThumbBmp(4, 6));
        HalFile thumb;
        assert(Storage.openFileForRead("test", x.getThumbBmpPath(4, 6), thumb));
        Bitmap bt(thumb);
        assert(bt.parseHeaders() == BmpReaderError::Ok);
        assert(bt.getWidth() == 4);
        assert(bt.getHeight() == 6);
      }
  std::cout << "PASS production XTC/XTCH mixed-container depth + 8/9-height payload/stream/bounds + cover and cropped "
               "thumbnail Bitmap readback\n";
}
