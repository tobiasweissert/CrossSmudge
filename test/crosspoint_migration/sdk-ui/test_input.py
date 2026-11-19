#!/usr/bin/env python3
"""Compile real input header plus extracted additive production methods.
Legacy mapping/RTC/GPIO bodies are doubles; this is not full input-stack QA.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from test_storage import method
ROOT=Path(__file__).resolve().parents[3]
def main():
    h=(ROOT/'src/MappedInputManager.h').read_text();s=(ROOT/'src/MappedInputManager.cpp').read_text()
    assert 'bool injectRemoteClick(RemoteButton button)' in h, 'Missing owner-compatible BLE input bridge'
    bodies='\n'.join(method(s,a) for a in ['bool MappedInputManager::isNavDirectionSwapped','MappedInputManager::Button MappedInputManager::mapScreenDirection','bool MappedInputManager::injectRemoteClick','bool MappedInputManager::wasLongPressed','bool MappedInputManager::consumeSuppressedRelease'])
    with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'],prefix='sdk-ui-input-') as td:
        d=Path(td)
        (d/'HalGPIO.h').write_text('''#pragma once
#include <cstdint>
#include <climits>
struct HalGPIO { bool edge=false,touch=false,release=false;
 bool wasAnyPressed()const{return edge;}bool wasAnyReleased()const{return release;}
 bool wasReleased(uint8_t)const{return release;}bool hasTouch()const{return touch;}
 void suppressTouchContact(){}
};
''')
        (d/'main.cpp').write_text(r'''
#include <array>
#include <cstddef>
#include <cstdint>
#include <cassert>
#define private public
#include "MappedInputManager.h"
#undef private
struct GfxRenderer {
 enum Orientation {Portrait,LandscapeClockwise,PortraitInverted,LandscapeCounterClockwise};
 Orientation rotation=Portrait; Orientation getOrientation()const{return rotation;}
};
struct Settings {bool frontButtonFollowOrientation=true;} settings;
#define SETTINGS settings
bool held=false,contact=false;unsigned long heldTime=0;
bool MappedInputManager::isPressed(Button)const{return held;}
bool MappedInputManager::isPhysicalPressed(Button)const{return held;}
bool MappedInputManager::isScreenContactActive()const{return contact;}
unsigned long MappedInputManager::getHeldTime()const{return heldTime;}
bool MappedInputManager::mapButton(Button,bool(HalGPIO::*fn)(uint8_t)const)const{return (gpio.*fn)(0);}
'''+bodies+r'''
int main(){HalGPIO gpio;GfxRenderer renderer;MappedInputManager input(gpio,renderer);
 using B=MappedInputManager::Button;
 assert(input.injectRemoteClick(RemoteButton::Next));assert(input.hasRemoteClick());
 assert(!input.injectRemoteClick(RemoteButton::Confirm));input.remoteInput.reset();
 held=true;assert(!input.injectRemoteClick(RemoteButton::Next));held=false;
 contact=true;assert(!input.injectRemoteClick(RemoteButton::Next));contact=false;
 gpio.edge=true;assert(!input.injectRemoteClick(RemoteButton::Next));gpio.edge=false;
 gpio.release=true;assert(!input.injectRemoteClick(RemoteButton::Next));gpio.release=false;
 assert(!input.injectRemoteClick(RemoteButton::None));
 const B expected[]={B::Left,B::Down,B::Right,B::Up};
 for(int r=0;r<4;++r){renderer.rotation=static_cast<GfxRenderer::Orientation>(r);assert(input.mapScreenDirection(B::ScreenLeft)==expected[r]);}
 settings.frontButtonFollowOrientation=false;assert(input.mapScreenDirection(B::ScreenLeft)==B::Left);
 held=true;heldTime=499;assert(!input.wasLongPressed(B::Confirm,500));heldTime=500;
 assert(input.wasLongPressed(B::Confirm,500));assert(!input.wasLongPressed(B::Confirm,500));
 gpio.release=true;assert(input.consumeSuppressedRelease());assert(input.consumeSuppressedRelease());
}
''')
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-DCROSSSMUDGE_APP_CAP_TOUCH=1','-I'+td,'-I'+str(ROOT/'src'),str(d/'main.cpp'),'-o',str(d/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(d/'probe')],check=True)
        print('PASS: real input header/additive methods; BLE collision/contact refusal, one event, four rotations, hold one-shot/frame suppression (doubles)')
if __name__=='__main__':main()
