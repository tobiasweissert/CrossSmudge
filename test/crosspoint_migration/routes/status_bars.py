#!/usr/bin/env python3
"""Actual seven-slot picker and settings config, popup/device boundary doubles."""
from pathlib import Path
import argparse, os, tempfile, subprocess, re, json
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
t=(R/'src/activities/settings/StatusBarSettingsActivity.cpp').read_text();h=(R/'src/CrossPointSettings.h').read_text();ids=sorted(set(re.findall(r'\bSTR_[A-Z0-9_]+',t)))
def method(signature):
 s=t.index(signature);return t[s:t.index('\n}',s)+2]
s='''#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <cstdio>
'''
s+=h[h.index('enum class ReaderStatusBarPosition'):h.index('struct ReaderStatusBarsPayload')]
s+='\nenum class StrId {'+','.join(ids)+'};\n'
s+='const char* idLabels[]={'+','.join('"'+i+'"' for i in ids)+'};\n'
s+='struct {const char* get(StrId id){return idLabels[static_cast<unsigned>(id)];}} I18N;\n#define tr(id) I18N.get(StrId::id)\n'
s+='''
struct CrossPointSettings {enum {XTC_STATUS_BAR_HIDE=0,XTC_STATUS_BAR_BOTTOM=1,XTC_STATUS_BAR_TOP=2,XTC_STATUS_BAR_MODE_COUNT=3,BOOK_PROGRESS=0,CHAPTER_PROGRESS=1,HIDE_PROGRESS=2,PROGRESS_BAR_THIN=0,PROGRESS_BAR_MEDIUM=1,PROGRESS_BAR_THICK=2,BOOK_PERCENTAGE_FORMAT_COUNT=3};ReaderStatusBarConfig topReaderStatusBar,bottomReaderStatusBar;DisplayStatusBarConfig displayStatusBar;uint8_t xtcStatusBarMode=0,legacyXtcTopUsesBottom=1;int saves=0;void saveToFile(){++saves;}} SETTINGS;
struct {bool available=true;bool isAvailable(){return available;}} halClock;
'''
s+=t[t.index('namespace {'):t.index('}  // namespace')+len('}  // namespace')]
s+='''
struct PopupDouble {std::vector<std::string> options;std::function<void(int)> cb;void show(StrId,const std::vector<std::string>& o,int,std::function<void(int)> f){options=o;cb=f;}};
struct StatusBarSettingsActivity {enum class View {Root,Top,Bottom};View view=View::Top;int selectedIndex=0;bool displayContext=false;PopupDouble optionPopup;void requestUpdate(){}void refreshItemCount(){}ReaderStatusBarPosition selectedPosition()const;void openOptionPicker();};
'''+method('ReaderStatusBarPosition StatusBarSettingsActivity::selectedPosition(')+'\n'+method('void StatusBarSettingsActivity::openOptionPicker(')
s+='''
int main(){StatusBarSettingsActivity a;for(int slot=0;slot<7;++slot){a.selectedIndex=slot;a.openOptionPicker();assert(a.optionPopup.options.size()==11);a.optionPopup.cb(2);assert(SETTINGS.topReaderStatusBar.slots[slot]==ReaderStatusBarItem::Battery);assert(SETTINGS.bottomReaderStatusBar.slots[slot]==ReaderStatusBarItem::Empty);}a.selectedIndex=7;a.openOptionPicker();a.optionPopup.cb(2);assert(SETTINGS.topReaderStatusBar.percentageFormat==2&&SETTINGS.bottomReaderStatusBar.percentageFormat==0);a.selectedIndex=8;a.openOptionPicker();a.optionPopup.cb(1);assert(SETTINGS.topReaderStatusBar.progressBar==CrossPointSettings::BOOK_PROGRESS);a.view=StatusBarSettingsActivity::View::Bottom;a.selectedIndex=3;a.openOptionPicker();a.optionPopup.cb(0);assert(SETTINGS.bottomReaderStatusBar.slots[3]==ReaderStatusBarItem::Clock);halClock.available=false;a.openOptionPicker();assert(a.optionPopup.options.size()==9);assert(SETTINGS.bottomReaderStatusBar.slots[3]==ReaderStatusBarItem::Clock);a.optionPopup.cb(-1);a.optionPopup.cb(99);assert(SETTINGS.bottomReaderStatusBar.slots[3]==ReaderStatusBarItem::Clock);a.view=StatusBarSettingsActivity::View::Root;a.openOptionPicker();assert(a.optionPopup.options.size()==3);a.optionPopup.cb(2);assert(SETTINGS.xtcStatusBarMode==2&&SETTINGS.legacyXtcTopUsesBottom==0);a.optionPopup.cb(3);assert(SETTINGS.xtcStatusBarMode==2);puts("PASS actual seven independent top/bottom slots; precision/progress raw domains; RTC loss retains choices; invalid picker values; stable XTC mode");}
'''
with tempfile.TemporaryDirectory(prefix='routes-bars-',dir=os.environ['TMPDIR']) as d:
 d=Path(d);(d/'bars.cpp').write_text(s);cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-O1','-g']
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 cmd += [str(d/'bars.cpp'),'-o',str(d/'bars')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;rc=p.returncode
 if not rc:
  p=subprocess.run([str(d/'bars')],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
 (a.evidence/'bars.log').write_text('COMMAND '+repr(cmd)+'\n'+log+'\nEXIT='+str(rc)+'\n');(a.evidence/'bars-results.json').write_text(json.dumps({'rc':rc,'passes':[l for l in log.splitlines() if l.startswith('PASS ')]},indent=2)+'\n');print(log);raise SystemExit(rc)
