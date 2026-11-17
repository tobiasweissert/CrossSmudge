#!/usr/bin/env python3
"""Execute production boot drawing, capture monochrome pixels in both orientations.
Text is stubbed: these are bitmap placement/polarity captures, not font/e-ink proof.
"""
from pathlib import Path
import re, subprocess, tempfile, hashlib, os
R = Path(__file__).resolve().parents[2]
s = (R/'src/activities/boot_sleep/BootActivity.cpp').read_text()
body = re.search(r'void drawDefaultBootLogo\(const GfxRenderer& renderer\) \{.*?\n\}', s, re.S).group()
includes = '\n'.join(l for l in s.splitlines() if 'images/' in l)
head = '''#include <cstdint>
#include <cstdio>
#include <cstring>
#define UI_10_FONT_ID 0
#define SMALL_FONT_ID 0
#define STR_CROSSSMUDGE "Crosscub"
#define STR_BOOTING "Booting"
#define CROSSSMUDGE_VERSION "test"
#define tr(x) x
namespace EpdFontFamily {enum {BOLD};}
struct GfxRenderer {
  int w,h; mutable uint8_t pixels[800*800];
  int getScreenWidth()const{return w;} int getScreenHeight()const{return h;}
  void clearScreen()const{memset(pixels,255,sizeof(pixels));}
  void drawPixel(int x,int y,bool black=true)const{if(x>=0&&x<w&&y>=0&&y<h)pixels[y*w+x]=black?0:255;}
  void drawImage(const uint8_t* b,int x,int y,int width,int height)const{
    for(int row=0;row<height;++row)for(int col=0;col<width;++col)
      drawPixel(x+col,y+row,!(b[row*((width+7)/8)+col/8]&(0x80>>(col%8))));
  }
  void drawCenteredText(int,int,const char*,bool=true,int=0)const{}
};
'''
with tempfile.TemporaryDirectory(prefix='crosscub-boot-') as tmp:
    p=Path(tmp);cpp=p/'boot.cpp'
    cpp.write_text(head+includes+'\n'+body+'\nint main(int argc,char**){GfxRenderer r;r.w=argc==1?480:800;r.h=argc==1?800:480;drawDefaultBootLogo(r);fwrite(r.pixels,1,r.w*r.h,stdout);}\n')
    subprocess.run(['g++','-std=c++17','-I'+str(R/'src'),str(cpp),'-o',str(p/'boot')],check=True)
    for w,h in [(480,800),(800,480)]:
        raw=subprocess.check_output([str(p/'boot')]+(['landscape'] if w==800 else []))
        cropped=bytes(raw[y*w+x] for y in range((h-120)//2,(h+120)//2) for x in range((w-120)//2,(w+120)//2))
        # Expected pixels are the supplied original bear's exact 1-bit bytes.
        original=(R/'src/images/CrosscubBear120.h')
        packed=bytes(int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})', original.read_text().split('{', 1)[1]))
        expected=bytes(255 if b&(0x80>>bit) else 0 for b in packed for bit in range(8))
        assert cropped==expected, f'{w}x{h}: production boot bitmap is not the supplied original Crosscub bear'
        out=os.environ.get('CROSSCUB_EVIDENCE')
        if out:
            dest=Path(out);dest.mkdir(parents=True,exist_ok=True)
            (dest/f'boot-logo-{w}x{h}.pgm').write_bytes(f'P5\n{w} {h}\n255\n'.encode()+raw)
        print(f'PASS boot bitmap {w}x{h}',hashlib.sha256(cropped).hexdigest())
