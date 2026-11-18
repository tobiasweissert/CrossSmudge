#!/usr/bin/env python3
"""Execute the real heatmap render function against draw-call geometry spies, not screenshots."""
from pathlib import Path
import argparse, os, subprocess, tempfile
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
cpp=(R/'src/activities/reader/BookStatsView.cpp').read_text()
render=cpp[cpp.index('void renderReadingHeatmapPage('):cpp.index('void renderGlobalStatsPage(')]
harness=r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "ReadingHeatmap.h"
constexpr int UI_10_FONT_ID=10,UI_12_FONT_ID=12;
namespace EpdFontFamily {constexpr int BOLD=1;}
struct GfxRenderer {
 struct Rect {int x,y,w,h;};struct Text {int x,y;std::string value;};
 int width,height;std::vector<Rect> fills,frames;std::vector<Text> texts;std::vector<std::pair<int,int>> pixels;
 void clearScreen(){}int getScreenWidth()const{return width;}int getScreenHeight()const{return height;}
 int getLineHeight(int)const{return 22;}int getTextWidth(int,const char* s)const{return std::string(s).size()*12;}
 void drawText(int,int x,int y,const char* s,bool=true,int=0){texts.push_back({x,y,s});}
 void fillRect(int x,int y,int w,int h,bool){fills.push_back({x,y,w,h});}
 void drawRect(int x,int y,int w,int h,bool){frames.push_back({x,y,w,h});}
 void drawPixel(int x,int y,bool){pixels.push_back({x,y});}
};
struct MappedInputManager {bool hasTouchHardware()const{return true;}std::string withBackArrow(const char* s)const{return s;}
 struct Labels {const char* btn1="",*btn2="",*btn3="",*btn4="";};template<class... Args>Labels mapLabels(Args...)const{return {};}};
struct TouchHeaderBackButton {static void drawCompact(GfxRenderer&,const char*,bool){}};
struct CompactHeader {static void drawTitle(GfxRenderer&,const char*){}};
struct Metrics {int contentSidePadding=20,topPadding=16;};
struct UITheme {static UITheme& getInstance(){static UITheme t;return t;}const Metrics& getMetrics(){static Metrics m;return m;}};
struct Layout {int topGap=12;};
Layout getStatsLayout(GfxRenderer&,const MappedInputManager*,bool,bool,bool){return {};}
int statsHeaderHeight(const Metrics&,const Layout&,const MappedInputManager*){return 48;}
struct Gui {template<class... Args>void drawButtonHints(Args...){}} GUI;
constexpr int STR_EXIT=0,STR_BACK=1,STR_MORE=2;
const char* tr(int){return "";}
struct GlobalReadingStats {uint32_t readingHistoryAnchorDay=0;std::array<uint8_t,READING_HISTORY_BYTES> readingHistoryBits{};};
bool validClock=true;
bool getCurrentLocalReadingStatsDateTime(ReadingStatsDateTime& now){now=validClock?ReadingStatsDateTime{{2024,3,1},12,0,0}:ReadingStatsDateTime{};return validClock;}
'''
harness+=render+r'''
int main(){
 for(int width:{480,800}){
  GlobalReadingStats stats;const auto today=readingStatsDayIndex({2024,3,1});
  for(unsigned n=0;n<365;++n)markReadingHistoryDay(stats.readingHistoryAnchorDay,stats.readingHistoryBits,today-n);
  auto before=stats.readingHistoryBits;MappedInputManager input;
  GfxRenderer r{width,width==480?800:480,{},{},{},{}};renderReadingHeatmapPage(r,&input,stats,true);
  std::set<int> rows;for(const auto& c:r.fills)rows.insert(c.y);
  assert(rows.size()==7); // Exactly one continuous week-column grid in BOTH orientations.
  assert(r.fills.size()==365 && r.frames.size()==1 && r.pixels.empty());
  assert(r.texts.size()<=17); // One count, small month/weekday labels, no paragraphs/legend/range.
  for(const auto& t:r.texts){assert(t.x>=0 && t.y>=64 && t.x+r.getTextWidth(10,t.value.c_str())<=width);}
  for(size_t i=0;i<r.texts.size();++i)for(size_t j=i+1;j<r.texts.size();++j){
   const auto& a=r.texts[i];const auto& b=r.texts[j];
   assert(a.x+r.getTextWidth(10,a.value.c_str())<=b.x || b.x+r.getTextWidth(10,b.value.c_str())<=a.x || a.y+22<=b.y || b.y+22<=a.y);
  }
  for(const auto& c:r.fills){assert(c.x>=0 && c.y>=64 && c.x+c.w<=width && c.y+c.h<r.height-60 && c.w>0 && c.h>0);}
  for(const auto& c:r.frames){assert(c.x>=0 && c.y>=64 && c.x+c.w<=width && c.y+c.h<r.height-60);}
  assert(stats.readingHistoryBits==before);
  GlobalReadingStats empty;GfxRenderer e{width,r.height,{},{},{},{}};renderReadingHeatmapPage(e,&input,empty,true);
  assert(e.fills.empty() && e.pixels.size()==365 && e.frames.size()==1);
  assert(e.texts.size()<=17);bool honestEmpty=false;for(const auto& t:e.texts)if(t.value=="No history")honestEmpty=true;assert(honestEmpty);
  validClock=false;GfxRenderer invalid{width,r.height,{},{},{},{}};renderReadingHeatmapPage(invalid,&input,empty,true);
  assert(invalid.fills.empty() && invalid.frames.empty() && invalid.pixels.empty());
  assert(invalid.texts.size()==1 && invalid.texts[0].value=="Set clock");validClock=true;
 }
 puts("PASS actual heatmap render: one grid portrait/landscape, 365 cells, bounded minimal labels, honest empty/invalid, read-only");
}
'''
with tempfile.TemporaryDirectory(prefix='heatmap-render-',dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')) as work:
 d=Path(work);(d/'render.cpp').write_text(harness)
 # Stub only SDK clock/settings headers for the real calendar utilities.
 (d/'HalClock.h').write_text('#pragma once\n#include <cstdint>\nstruct Clock { bool getDateTime(uint16_t&,uint8_t&,uint8_t&,uint8_t&,uint8_t&){return false;} }; inline Clock halClock;\n')
 (d/'CrossPointSettings.h').write_text('#pragma once\nstruct Settings { int clockUtcOffsetQ=48; }; inline Settings SETTINGS;\n')
 # The probe supplies the clock function, so compile production utility under a private renamed clock symbol.
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-include','cstdio','-DgetCurrentLocalReadingStatsDateTime=unusedProductionClock','-I'+str(d),'-I'+str(R/'src/activities/reader'),'-c',str(R/'src/activities/reader/ReadingStatsUtils.cpp'),'-o',str(d/'utils.o')],check=True)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I'+str(R/'src/activities/reader'),str(d/'render.cpp'),str(d/'utils.o'),'-o',str(d/'render')],check=True)
 subprocess.run([str(d/'render')],check=True)
