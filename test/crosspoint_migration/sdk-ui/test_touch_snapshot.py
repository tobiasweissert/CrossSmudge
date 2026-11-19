#!/usr/bin/env python3
"""Production touch-snapshot behavior; input and SDK types are host doubles."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    text=(ROOT/'src/components/UiAppHelpers.h').read_text()
    anchor='inline freeink::ui::InputSnapshot touchSnapshotFrom('
    start=text.index(anchor);depth=0;b=text.index('{',start)
    end = b
    for end in range(b,len(text)):
        if text[end]=='{':depth+=1
        if text[end]=='}':
            depth-=1
            if depth==0:break
    body=text[start:end+1]
    source=r'''
#include <cstdint>
#include <cassert>
namespace freeink::ui {struct InputSnapshot {bool touchPressed=false,touchReleased=false,touchHeld=false,longPress=false;int16_t touchX=0,touchY=0;};}
struct MappedInputManager {
 enum class SwipeDir {None,Up};
 bool longPress=false,down=false,tap=false,held=false,released=false,swipe=false;
 bool wasScreenLongPress(int& x,int& y)const{x=12;y=34;return longPress;}
 bool wasScreenTouchDown(int& x,int& y)const{x=10;y=20;return down;}
 bool isScreenTouchHeld(int& x,int& y)const{x=11;y=22;return held;}
 bool wasScreenTapped(int& x,int& y)const{x=13;y=24;return tap;}
 bool wasScreenTouchReleased()const{return released;}
 SwipeDir wasSwipe()const{return swipe?SwipeDir::Up:SwipeDir::None;}
};
'''+body+r'''
int main(){MappedInputManager i;
 auto s=touchSnapshotFrom(i,false);assert(!s.touchReleased&&!s.touchPressed&&!s.touchHeld);
 i.longPress=true;s=touchSnapshotFrom(i,false);assert(!s.longPress);
 s=touchSnapshotFrom(i,true);assert(s.longPress&&s.touchReleased&&s.touchX==12&&s.touchY==34);
 i.longPress=false;i.held=true;s=touchSnapshotFrom(i,false);assert(s.touchHeld&&s.touchX==11);
 i.held=false;i.released=true;s=touchSnapshotFrom(i,false);assert(s.touchReleased&&s.touchX==-1&&s.touchY==-1);
 i.released=false;i.swipe=true;s=touchSnapshotFrom(i,false);assert(s.touchReleased&&s.touchX==-1);
 i.swipe=false;i.tap=true;s=touchSnapshotFrom(i,false);assert(s.touchReleased&&s.touchX==13);
}
'''
    with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'],prefix='sdk-ui-touch-') as td:
        p=Path(td);(p/'main.cpp').write_text(source)
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',str(p/'main.cpp'),'-o',str(p/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(p/'probe')],check=True)
        print('PASS: production snapshot idle, opt-in long press, drag, raw release, swipe-cancel, tap coordinates')
if __name__=='__main__':main()
