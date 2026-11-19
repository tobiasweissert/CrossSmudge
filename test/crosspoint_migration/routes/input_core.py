#!/usr/bin/env python3
"""Real clock/UTF-8/view-fence bodies, with declared synthetic input doubles."""
from pathlib import Path
import argparse, os, tempfile, subprocess, json
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
def method(path,signature):
 t=(R/path).read_text();s=t.index(signature);return t[s:t.index('\n}',s)+2]
clock='src/activities/settings/ClockOffsetActivity.cpp';keyboard='src/activities/util/KeyboardEntryActivity.cpp'
s=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <cassert>
#include <cstdio>
constexpr uint8_t BIAS_QUARTER_HOURS=48,MAX_POS_HOURS=14,MAX_NEG_HOURS=12,MINUTE_STEPS=4;
struct {uint8_t clockUtcOffsetQ=48,clockTimezone=17;int saves=0;void saveToFile(){++saves;}} SETTINGS;
namespace timezones {int applied=0;void applyToClock(){++applied;}}
struct ClockOffsetActivity {enum Field{FIELD_SIGN,FIELD_HOURS,FIELD_MINUTES,FIELD_COUNT};Field activeField=FIELD_SIGN;uint8_t sign=0,hours=0,minutesQuarter=0;void clampForSign();void adjustActiveField(int);void saveToSettings()const;};
'''
for sig in ['uint8_t encodeOffset(','void decodeOffset(','void ClockOffsetActivity::clampForSign(','void ClockOffsetActivity::adjustActiveField(','void ClockOffsetActivity::saveToSettings(']:s+='\n'+method(clock,sig)
s+=r'''
struct KeyboardEntryActivity {std::string text;size_t cursorPos=0,maxLength=0;static size_t utf8Prev(const std::string&,size_t);static size_t utf8Next(const std::string&,size_t);void insertUtf8(const char*);bool backspaceUtf8();};
'''
for sig in ['size_t KeyboardEntryActivity::utf8Prev(','size_t KeyboardEntryActivity::utf8Next(','void KeyboardEntryActivity::insertUtf8(','bool KeyboardEntryActivity::backspaceUtf8(']:s+='\n'+method(keyboard,sig)
s+=r'''
struct MappedInputManager {enum class Button {Back,Confirm,Left,Right,Up,Down,Power};bool down=false,tap=false,release=false,contact=false,pressed=false,released=false,physical=false;bool wasScreenTouchDown(int&,int&){return down;}bool wasScreenTapped(int&,int&){return tap;}bool isScreenContactActive(){return contact;}bool isPhysicalPressed(Button){return physical;}bool wasAnyPressed(){return pressed;}bool wasAnyReleased(){return released;}bool wasScreenTouchReleased(){return release;}bool wasPressed(Button b){return pressed&&b==Button::Confirm;}bool wasReleased(Button b){return released&&b==Button::Confirm;}};
'''
s+='\n'+'\n'.join(l for l in (R/'src/activities/home/FileMutationInputGate.h').read_text().splitlines() if not l.startswith('#'))
s+=r'''
int main(){for(int value=0;value<=104;++value){uint8_t sign,h,q;decodeOffset(value,sign,h,q);assert(encodeOffset(sign,h,q)==value);}ClockOffsetActivity c;c.saveToSettings();assert(SETTINGS.clockTimezone==17&&SETTINGS.saves==0);c.hours=5;c.minutesQuarter=3;c.saveToSettings();assert(SETTINGS.clockTimezone==255&&SETTINGS.clockUtcOffsetQ==71&&timezones::applied==1);c.hours=14;c.clampForSign();assert(c.minutesQuarter==0);c.sign=1;c.clampForSign();assert(c.hours==12);puts("PASS actual manual UTC picker all quarter-hour encodings, named-zone no-edit retention, explicit override and bounds");KeyboardEntryActivity k;for(int n=0;n<900;++n)k.text+="abc\n\xc3\xa9\xd7\x90";const auto original=k.text;k.cursorPos=k.text.size();while(k.cursorPos)k.cursorPos=k.utf8Prev(k.text,k.cursorPos);assert(k.cursorPos==0);k.insertUtf8("\xe2\x82\xac");assert(k.backspaceUtf8()&&k.text==original);while(k.cursorPos<k.text.size())k.cursorPos=k.utf8Next(k.text,k.cursorPos);assert(k.text==original);k.maxLength=k.text.size();k.insertUtf8("z");assert(k.text==original);puts("PASS actual long LF/multibyte draft UTF-8 traversal/edit primitives preserve all bytes (not physical cursor reachability)");FileMutationInputGate gate;MappedInputManager i;assert(!gate.allowInput(i));gate.beginPaint();assert(!gate.allowInput(i));gate.endPaint();i.released=true;assert(!gate.allowInput(i));i.released=false;assert(!gate.allowInput(i));assert(gate.allowInput(i));i.pressed=true;assert(gate.allowInput(i));i.pressed=false;i.released=true;assert(gate.allowInput(i));gate.beginPaint();assert(!gate.allowInput(i));gate.endPaint();assert(!gate.allowInput(i));i.released=false;assert(!gate.allowInput(i));i.contact=true;i.tap=true;assert(!gate.allowInput(i));i.contact=i.tap=false;assert(gate.allowInput(i));i.contact=i.down=true;assert(gate.allowInput(i));i.contact=i.down=false;i.tap=i.release=true;assert(gate.allowInput(i));i.tap=i.release=false;assert(gate.allowInput(i));puts("PASS actual shared view fence rejects unpainted/stale release; neutral plus new hardware/touch; idle dismissal remains reachable");}
'''
with tempfile.TemporaryDirectory(prefix='routes-input-',dir=os.environ['TMPDIR']) as d:
 d=Path(d);(d/'input.cpp').write_text(s);cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g']
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 cmd += [str(d/'input.cpp'),'-o',str(d/'input')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;rc=p.returncode
 if not rc:
  p=subprocess.run([str(d/'input')],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
 (a.evidence/'input.log').write_text('COMMAND '+repr(cmd)+'\n'+log+'\nEXIT='+str(rc)+'\n');(a.evidence/'input-results.json').write_text(json.dumps({'rc':rc,'passes':[l for l in log.splitlines() if l.startswith('PASS ')]},indent=2)+'\n');print(log);raise SystemExit(rc)
