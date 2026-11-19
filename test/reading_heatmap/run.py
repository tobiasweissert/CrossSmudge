#!/usr/bin/env python3
"""Executable production calendar + Activity-loop slices, synthetic input/SDK boundary."""
from pathlib import Path
import argparse, subprocess, tempfile, os
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='reading-heatmap-',dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')) as d:
 d=Path(d)
 (d/'HalClock.h').write_text('#pragma once\n#include <cstdint>\nstruct Clock { bool getDateTime(uint16_t&,uint8_t&,uint8_t&,uint8_t&,uint8_t&){return false;} }; inline Clock halClock;\n')
 (d/'CrossPointSettings.h').write_text('#pragma once\nstruct Settings { int clockUtcOffsetQ=48; }; inline Settings SETTINGS;\n')
 source=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include "ReadingHeatmap.h"
int main(){
 std::array<uint8_t,READING_HISTORY_BYTES> bits{};
 const ReadingStatsDate today{2024,3,1}; auto day=readingStatsDayIndex(today);
 markReadingHistoryDay(day,bits,day); markReadingHistoryDay(day,bits,day-1);
 auto m=ReadingHeatmap::build(today,day,bits);
 assert(m.valid && m.lastDay==day && m.firstDay==day-364 && m.activeDays==2);
 assert(m.cell(m.todayColumn,m.todayRow)==ReadingHeatmap::Cell::Active);
 assert(m.todayRow==4); // Friday, Monday-first
 assert(m.cell(m.todayColumn,5)==ReadingHeatmap::Cell::Outside);
 assert(m.columns==53 && m.cell(0,0)==ReadingHeatmap::Cell::Outside);
 puts("PASS leap-day, 365-day period, Monday alignment, future padding");
 const auto before=bits;
 // Production uses anchor zero AND bit zero clear as its empty-history sentinel.
 std::array<uint8_t,READING_HISTORY_BYTES> emptyBits{};
 auto missing=ReadingHeatmap::build(today,0,emptyBits);assert(missing.knownDays==0 && missing.activeDays==0);
 emptyBits[0]=2; // Unrelated older bits do not make the sentinel valid.
 auto unrelated=ReadingHeatmap::build(today,0,emptyBits);assert(unrelated.knownDays==0 && unrelated.activeDays==0);
 uint32_t epochAnchor=0;std::array<uint8_t,READING_HISTORY_BYTES> epochBits{};
 markReadingHistoryDay(epochAnchor,epochBits,0);
 assert(epochAnchor==0 && epochBits[0]==1);
 const auto epochBefore=epochBits;
 auto epoch=ReadingHeatmap::build({2000,1,1},epochAnchor,epochBits);
 assert(epoch.valid && epoch.activeDays==1 && epoch.knownDays==1);
 assert(epoch.cell(epoch.todayColumn,epoch.todayRow)==ReadingHeatmap::Cell::Active);
 assert(epoch.cell(0,4)==ReadingHeatmap::Cell::Outside && epoch.cell(0,6)==ReadingHeatmap::Cell::Outside);
 auto epochNext=ReadingHeatmap::build({2000,1,2},epochAnchor,epochBits);
 assert(epochNext.activeDays==1 && epochNext.knownDays==2);
 assert(epochNext.cell(epochNext.todayColumn,epochNext.todayRow)==ReadingHeatmap::Cell::Inactive);
 auto epochStale=ReadingHeatmap::build(today,epochAnchor,epochBits);
 assert(epochStale.activeDays==0 && epochStale.knownDays==365);
 auto invalidAnchor=ReadingHeatmap::build(today,UINT32_MAX,epochBits);
 assert(invalidAnchor.activeDays==0 && invalidAnchor.knownDays==0);
 assert(epochAnchor==0 && epochBits==epochBefore);
 puts("PASS RH-1: production epoch history, empty sentinel, older bits, padding, next day, stale epoch, invalid anchor, read-only");
 auto invalid=ReadingHeatmap::build({},day,bits);assert(!invalid.valid);
 auto expired=ReadingHeatmap::build(today,day+730,bits);assert(expired.knownDays==0);
 auto boundary=ReadingHeatmap::build(today,day+729,bits);assert(boundary.knownDays==1);
 auto stale=ReadingHeatmap::build(today,day-1000,bits);assert(stale.knownDays==365 && stale.activeDays==0);
 assert(bits==before);
 for(unsigned n=0;n<365;++n)markReadingHistoryDay(day,bits,day-n);
 auto full=ReadingHeatmap::build(today,day,bits);assert(full.activeDays==365);
 assert(full.cell(-1,0)==ReadingHeatmap::Cell::Outside && full.cell(0,7)==ReadingHeatmap::Cell::Outside);
 puts("PASS empty/invalid clock, retention boundary, stale anchor, read-only history, full period");
 /*CALENDAR_ORACLE*/
}
'''
 import datetime
 oracle=[]
 for year in range(2000,2100):
  for month,day_of_month in [(1,1),(2,28),(2,29),(3,1),(12,31)]:
   try:date=datetime.date(year,month,day_of_month)
   except ValueError:continue
   index=(date-datetime.date(2000,1,1)).days
   oracle.append('{'+f'{year},{month},{day_of_month},{index},{date.weekday()}'+'}')
 source=source.replace('/*CALENDAR_ORACLE*/','''
 struct Oracle {int year,month,day,index,weekday;};
 const Oracle cases[]={'''+','.join(oracle)+'''};
 for(const auto& c:cases){
  ReadingStatsDate date{static_cast<uint16_t>(c.year),static_cast<uint8_t>(c.month),static_cast<uint8_t>(c.day)};
  assert(readingStatsDayIndex(date)==static_cast<unsigned>(c.index));
  auto grid=ReadingHeatmap::build(date,c.index,bits);
  assert(grid.valid && grid.todayRow==c.weekday && grid.columns<=53);
  unsigned inside=0;
  for(int col=0;col<grid.columns;++col)for(int row=0;row<7;++row){
   if(grid.cell(col,row)!=ReadingHeatmap::Cell::Outside){++inside;auto index=grid.gridStart+col*7+row;assert(index>=static_cast<int>(grid.firstDay)&&index<=c.index);}
  }
  assert(inside==static_cast<unsigned>(std::min(c.index+1,365)));
 }
 puts("PASS Gregorian oracle: 425 boundary/leap cases across 2000-2099");
 ''')
 source='#include <algorithm>\n'+source
 (d/'test.cpp').write_text(source)
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-include','cstdio','-I'+str(d),'-I'+str(R/'src/activities/reader'),str(d/'test.cpp'),str(R/'src/activities/reader/ReadingStatsUtils.cpp'),'-o',str(d/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(d/'test')],check=True)

 cpp=(R/'src/activities/reader/BookStatsActivity.cpp').read_text()
 header=(R/'src/activities/reader/BookStatsActivity.h').read_text()
 def method(name):
  start=cpp.index(name+'(');start=cpp.rfind('\n',0,start)+1
  end=cpp.index('\n}\n',start)+3
  return cpp[start:end].replace('BookStatsActivity::','')
 enum=header[header.index('enum class Page'):header.index(';',header.index('enum class Page'))+1]
 harness=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include <string>
struct GfxRenderer { enum class Orientation {Portrait,LandscapeClockwise}; Orientation getOrientation(){return Orientation::Portrait;}void setOrientation(Orientation){} };
struct Activity {void onEnter(){}};
struct MappedInputManager {
 enum class Button {Back,Confirm,Power,Up,Left,Down,Right}; enum class SwipeDir {None,Left,Right};
 Button pressed=Button::Power,released=Button::Power; SwipeDir swipe=SwipeDir::None; int target=-1; bool header=false;
 bool isPressed(Button){return false;}bool wasPressed(Button b){return b==pressed;}bool wasReleased(Button b){return b==released;}
 bool wasItemTapped(int& t){t=target;return target>=0;}bool wasItemTouchedDown(int&){return false;}
 SwipeDir wasSwipe(){return swipe;}void suppressNextBackRelease(){}void suppressNextConfirmRelease(){}
};
struct InputReleaseGuard {static bool consumeInitialRelease(MappedInputManager&,MappedInputManager::Button,bool&){return false;}};
struct TouchHeaderBackButton {static int compactHeaderRect(GfxRenderer&){return 0;}static bool wasTapped(MappedInputManager& i,int){return i.header;}};
namespace BookStatsTouchTarget {constexpr int PageIndicatorBase=100,DateFieldBase=10,DateFieldCount=6,DateAdjustUp=20,DateAdjustDown=21,DateSave=22,DateCancel=23,StartedDaysStat=1;}
struct Harness : Activity {
'''+enum+r'''
 Page page=INITIAL_PAGE;MappedInputManager mappedInput;GfxRenderer renderer;int updates=0,exits=0,edits=0,selectedEditField=0;
 std::string bookCachePath="/SAMPLE/cache";GfxRenderer::Orientation previousOrientation=GfxRenderer::Orientation::Portrait;
 bool showAllDevicesStats=false,returnToHomeOnExit=false,noRtc=false,editable=true;
 bool ignoreInitialBackRelease=false,ignoreInitialPowerRelease=false,ignoreInitialConfirmRelease=false;
 bool usesNoRtcSingleScreenLayout(){return noRtc;}bool hasEditableBook(){return editable;}
 void requestUpdate(){++updates;}void finish(){++exits;}void onGoHome(){++exits;}
 void beginDateEditing(){page=Page::EditDates;++edits;}void finishDateEditing(bool){page=Page::PerBook;}void adjustSelectedDateField(int){}void cycleEditField(){}
'''
 import re
 initial_page=re.search(r'Page page = (Page::\w+);',header)
 assert initial_page
 harness=harness.replace('INITIAL_PAGE',initial_page.group(1))
 for name in ['onEnter','exitStatsActivity','showNextStatsPage','showPreviousStatsPage','selectEditFieldFromTouchTarget','loop']:harness+=method(name)
 harness+=r'''
};
int main(){
 Harness initial;initial.onEnter();assert(initial.page==Harness::Page::Heatmap);
 Harness noBook;noBook.bookCachePath.clear();noBook.onEnter();assert(noBook.page==Harness::Page::Heatmap);
 for(bool aggregate:{false,true}){
 Harness h;h.showAllDevicesStats=aggregate;
 assert(h.page==Harness::Page::Heatmap && static_cast<int>(h.page)==0);
 h.mappedInput.pressed=MappedInputManager::Button::Right;h.loop();assert(h.page==Harness::Page::PerBook);
 h.mappedInput.pressed=MappedInputManager::Button::Left;h.loop();assert(h.page==Harness::Page::Heatmap && h.edits==0);
 h.mappedInput.pressed=MappedInputManager::Button::Right;h.loop();assert(h.page==Harness::Page::PerBook);
 h.loop();assert(h.page==Harness::Page::ThisDevice && static_cast<int>(h.page)==2);
 h.loop();assert(static_cast<int>(h.page)==(aggregate?3:2));
 h.mappedInput.pressed=MappedInputManager::Button::Left;h.loop();assert(static_cast<int>(h.page)==(aggregate?2:1));
 h.page=Harness::Page::Heatmap;h.mappedInput.pressed=MappedInputManager::Button::Power;h.mappedInput.swipe=MappedInputManager::SwipeDir::Left;
 h.loop();assert(h.page==Harness::Page::PerBook);
 h.mappedInput.swipe=MappedInputManager::SwipeDir::Right;h.loop();assert(h.page==Harness::Page::Heatmap);
 h.page=Harness::Page::Heatmap;h.mappedInput.swipe=MappedInputManager::SwipeDir::None;
 h.mappedInput.released=MappedInputManager::Button::Back;h.loop();assert(h.exits==1);
 h.mappedInput.released=MappedInputManager::Button::Confirm;h.loop();assert(h.exits==2 && h.edits==0);
 h.mappedInput.released=MappedInputManager::Button::Power;h.mappedInput.header=true;h.loop();assert(h.exits==3);
 }
 Harness touch;touch.mappedInput.target=BookStatsTouchTarget::PageIndicatorBase+2;touch.loop();assert(static_cast<int>(touch.page)==2);
 touch.mappedInput.target=BookStatsTouchTarget::PageIndicatorBase+3;touch.loop();assert(static_cast<int>(touch.page)==2);
 touch.showAllDevicesStats=true;touch.loop();assert(touch.page==Harness::Page::AllDevices);
 touch.mappedInput.target=BookStatsTouchTarget::PageIndicatorBase;touch.loop();assert(touch.page==Harness::Page::Heatmap);
 touch.mappedInput.target=BookStatsTouchTarget::PageIndicatorBase+1;touch.loop();assert(touch.page==Harness::Page::PerBook);
 Harness legacy;legacy.noRtc=true;legacy.mappedInput.pressed=MappedInputManager::Button::Right;legacy.loop();assert(legacy.page==Harness::Page::Heatmap);
 legacy.mappedInput.released=MappedInputManager::Button::Confirm;legacy.loop();assert(legacy.exits==1 && legacy.edits==0);
 Harness edit;edit.page=Harness::Page::PerBook;edit.mappedInput.target=BookStatsTouchTarget::StartedDaysStat;edit.loop();assert(edit.page==Harness::Page::EditDates);
 Harness confirm;confirm.page=Harness::Page::PerBook;confirm.mappedInput.released=MappedInputManager::Button::Confirm;confirm.loop();assert(confirm.page==Harness::Page::EditDates);
 Harness back;back.mappedInput.swipe=MappedInputManager::SwipeDir::Right;back.loop();assert(back.exits==1);
 puts("PASS actual Activity loop: hardware, swipes, header/back/confirm, editor route, no-RTC preservation");
}
'''
 (d/'activity.cpp').write_text(harness)
 subprocess.run(['g++','-std=c++17',*flags,str(d/'activity.cpp'),'-o',str(d/'activity')],check=True)
 subprocess.run([str(d/'activity')],check=True)

import sys
subprocess.run([sys.executable,str(Path(__file__).with_name('render_probe.py'))]+(['--sanitize'] if a.sanitize else []),check=True)

# Optional full production input path, using an already-built hash-bound launch fixture.
# No build or dependency installation is triggered by the routine suite.
if os.environ.get('READING_HEATMAP_SIMULATOR'):
 import sys
 output=os.environ.get('READING_HEATMAP_SIM_OUTPUT',str(Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/'reading-heatmap-sdl'))
 subprocess.run([sys.executable,str(Path(__file__).with_name('simulator_input.py')),
                 '--simulator',os.environ['READING_HEATMAP_SIMULATOR'],'--output',output],check=True)
