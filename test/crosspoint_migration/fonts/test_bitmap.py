#!/usr/bin/env python3
"""Execute production Bitmap over in-memory storage, not device/SD pixels."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[3]
class BitmapCompatibility(unittest.TestCase):
    def test_v4_palette_raw_gray_and_owner_downsampling(self):
        storage=r'''
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
class HalFile {
 public:
 std::vector<uint8_t> bytes; size_t pos=0;
 explicit operator bool() const { return true; }
 bool seek(size_t n) { if(n>bytes.size()) return false; pos=n; return true; }
 bool seekCur(int n) { return seek(pos+n); }
 int read() { return pos<bytes.size()?bytes[pos++]:-1; }
 int read(void* dst,size_t n) { n=std::min(n,bytes.size()-pos); std::memcpy(dst,bytes.data()+pos,n); pos+=n; return n; }
 size_t position() const { return pos; }
};
'''
        source=r'''
#include "Bitmap.h"
#include <cassert>
int adjustPixel(int g) { return g; }
uint8_t quantize(int g,int,int) { return g>>6; }
static void le(std::vector<uint8_t>& b,int offset,uint32_t n,int len=4) {
 for(int i=0;i<len;++i) b[offset+i]=n>>(8*i);
}
int main() {
 HalFile f; f.bytes.resize(134);
 le(f.bytes,0,0x4D42,2); le(f.bytes,10,130); le(f.bytes,14,108);
 le(f.bytes,18,2); le(f.bytes,22,1); le(f.bytes,26,1,2); le(f.bytes,28,1,2);
 // V4 palette begins at 14+108, NOT at offset 54.
 f.bytes[126]=255; f.bytes[127]=255; f.bytes[128]=255;
 f.bytes[130]=0x40;
 Bitmap b(f); assert(b.parseHeaders()==BmpReaderError::Ok);
 uint8_t gray[2]={99,99}, row[8]={};
 assert(b.readRowGray(0,gray,row)==BmpReaderError::Ok);
 assert(gray[0]==0 && gray[1]==255);
 assert(b.readRowGray(-1,gray,row)==BmpReaderError::BadDimensions);
 assert(b.rewindToData()==BmpReaderError::Ok);
 uint8_t packed[1]={}; assert(b.readNextRow(packed,row)==BmpReaderError::Ok);
 assert(packed[0]==0x30);
 // Owner downsizing remains exposed alongside the new raw-gray path.
 assert(!b.setDitheredOutputSize(1,1)); // no high-color ditherer
 HalFile hi; hi.bytes.resize(78);
 le(hi.bytes,0,0x4D42,2); le(hi.bytes,10,54); le(hi.bytes,14,40);
 le(hi.bytes,18,4); le(hi.bytes,22,static_cast<uint32_t>(-2));
 le(hi.bytes,26,1,2); le(hi.bytes,28,24,2);
 // Bottom display row has neutral grayscale samples 0,85,170,255.
 for(int x=0;x<4;++x) for(int c=0;c<3;++c) hi.bytes[66+3*x+c]=x*85;
 Bitmap resized(hi,true,true); assert(resized.parseHeaders()==BmpReaderError::Ok);
 assert(resized.setDitheredOutputSize(2,1));
 assert(resized.getWidth()==2 && resized.getHeight()==1);
 const auto before=hi.position(); uint8_t rowHi[12]={};
 assert(resized.readRowGray(0,gray,rowHi)==BmpReaderError::Ok);
 assert(gray[0]==85 && gray[1]==255);
 assert(hi.position()==before); // raw access cannot disturb next sequential read
 assert(resized.readNextRow(packed,rowHi)==BmpReaderError::Ok);
 assert(resized.readNextRow(packed,rowHi)==BmpReaderError::ShortReadRow);
 assert(resized.rewindToData()==BmpReaderError::Ok);
 assert(resized.readNextRow(packed,rowHi)==BmpReaderError::Ok);
 AtkinsonDitherer image(2,true); assert(image.isValid());
 assert(image.processPixel(85,0)==1); image.reset();
 assert(image.processPixel(170,0)==2);
 AtkinsonDitherer invalid(0,true); assert(!invalid.isValid());
 assert(invalid.processPixel(255,0)==3);
 for(int l=0;l<4;++l) {
  auto a=grayPlanePixel(l,false,true); assert(a.write);
  auto o=grayPlanePixel(l,true,false); assert(o.write==(l==1||l==2));
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='bitmap-',dir=os.environ.get('TMPDIR')) as d:
            p=Path(d); (p/'HalStorage.h').write_text(storage)
            (p/'Logging.h').write_text('#pragma once\n#define LOG_ERR(...) ((void)0)\n#define LOG_DBG(...) ((void)0)\n')
            (p/'probe.cpp').write_text(source)
            cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',
                 '-I'+d,'-I'+str(ROOT/'lib/GfxRenderer'),str(p/'probe.cpp'),
                 str(ROOT/'lib/GfxRenderer/Bitmap.cpp'),'-o',str(p/'probe')]
            if os.environ.get('FONT_SANITIZE'):
                cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(cmd,check=True); subprocess.run([str(p/'probe')],check=True)
if __name__=='__main__': unittest.main()
