#!/usr/bin/env python3
"""Compile real theme declarations and exercise additive cover clipping primitive.
No production fonts/pixels/device visual acceptance is implied.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from test_storage import method
ROOT=Path(__file__).resolve().parents[3]
def main():
    body=method((ROOT/'src/components/themes/BaseTheme.cpp').read_text(),'bool BaseTheme::drawCoverThumbFill(')
    header_rect=method((ROOT/'src/components/UITheme.cpp').read_text(),'Rect UITheme::getHeaderRect(')
    with tempfile.TemporaryDirectory(prefix='sdk-ui-themes-',dir=os.environ['TMPDIR']) as td:
        d=Path(td)
        (d/'Icon.h').write_text('#pragma once\nnamespace freeink {struct Icon;}\n')
        (d/'main.cpp').write_text(r'''
#include <algorithm>
#include <array>
#include <cassert>
#include "components/themes/BaseTheme.h"
#include "components/themes/lyra/LyraTheme.h"
#include "components/themes/lyra/Lyra3CoversTheme.h"
#include "components/themes/roundedraff/RoundedRaffTheme.h"
static_assert(BaseMetrics::values.headerHeight==70);
static_assert(LyraMetrics::values.headerHeight==84);
static_assert(RoundedRaffMetrics::values.headerHeight==70);
static_assert(UIIcon::Applications==14 && UIIcon::BookmarkIcon==5);
static_assert(UIIcon::Bookmark==UIIcon::BookmarkIcon);
struct Bitmap {int getWidth()const{return 80;}int getHeight()const{return 100;}};
struct GfxRenderer {mutable std::array<int,4> clip{0,0,480,800};mutable int x=0,y=0;mutable std::array<int,4> paintedClip{};
 auto getClipRect()const{return clip;}
 void setClipRect(int a,int b,int c,int d)const{clip={a,b,c,d};}
 bool drawBitmap(const Bitmap&,int a,int b,int,int)const{x=a;y=b;paintedClip=clip;return true;}
};
struct UITheme {
 Rect getHeaderRect(const GfxRenderer&,bool,bool);
 Rect getScreenSafeArea(const GfxRenderer&,bool,bool){return Rect{3,4,470,780};}
 static Rect getContentArea(const GfxRenderer&){return Rect{10,11,460,778};}
 ThemeMetrics getMetrics(){return BaseMetrics::values;}
};
'''+body+'\n'+header_rect+r'''
int main(){GfxRenderer r;Bitmap bitmap;const auto old=r.clip;
 UITheme theme;const Rect header=theme.getHeaderRect(r,false,false);assert(header.x==3&&header.y==9&&header.width==470&&header.height==70);
 assert(BaseTheme::drawCoverThumbFill(r,bitmap,Rect{20,30,60,70},4));
 assert(r.x==14&&r.y==15);assert((r.paintedClip==std::array<int,4>{20,30,60,70}));assert(r.clip==old);
 assert(!BaseTheme::drawCoverThumbFill(r,bitmap,Rect{0,0,0,70},0));assert(r.clip==old);
}
''')
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I'+td,'-I'+str(ROOT/'src'),str(d/'main.cpp'),'-o',str(d/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(d/'probe')],check=True)
        print('PASS: actual theme headers/overrides/initializer order, owner header geometry/icon IDs, additive cover clipping/restoration (renderer double)')
if __name__=='__main__':main()
