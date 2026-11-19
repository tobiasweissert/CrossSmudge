#!/usr/bin/env python3
"""Real UiAppHost header/implementation with SDK/render/input doubles."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    source=(ROOT/'src/components/UiAppHost.cpp').read_text()
    assert '<<<<<<<' not in source
    source='\n'.join(l for l in source.splitlines() if not l.startswith('#include'))
    with tempfile.TemporaryDirectory(prefix='sdk-ui-host-',dir=os.environ['TMPDIR']) as td:
        d=Path(td)
        (d/'FreeInkApp.h').write_text(r'''#pragma once
#include <cstddef>
namespace freeink::ui {
 struct InputSnapshot {bool touchPressed=false,touchReleased=false,touchHeld=false,swipeLeft=false,swipeRight=false;};
 struct ActionEvent {bool longPress=false;bool active=false;explicit operator bool()const{return active;}};
 template<size_t N,size_t M>struct FreeInkApp {
 struct ScreenType{};int renders=0,routes=0;
 template<typename T>FreeInkApp(T&,int){}void setDevice(int){}void render(){++renders;}
 ActionEvent route(const InputSnapshot&){++routes;return {false,true};}
 };
}
''')
        (d/'FreeInkUIGfxRenderer.h').write_text(r'''#pragma once
namespace freeink::ui {struct GfxRendererTarget {int deviceContext(){return 0;}};}
''')
        (d/'main.cpp').write_text(r'''
#include "UiAppHost.h"
#include <cassert>
struct GfxRenderer{};struct MappedInputManager{};
namespace fui=freeink::ui;
fui::InputSnapshot pending;
fui::GfxRendererTarget makeUiTarget(const GfxRenderer&){return {};}
void applySharedUiTheme(UiAppHost::UiApp&,const fui::GfxRendererTarget&){}
fui::InputSnapshot touchSnapshotFrom(const MappedInputManager&,bool){return pending;}
namespace haptic_feedback {void touchAction(bool){}}
'''+source+r'''
int main(){GfxRenderer renderer;MappedInputManager input;UiAppHost host(renderer);
 assert(&host.target==&host.uiTarget);
 pending.touchReleased=true;assert(!host.routeTouch(input).routed);assert(host.app.routes==0);
 host.renderUi();assert(host.routingReady());assert(host.routeTouch(input).routed);assert(host.app.routes==1);
 host.renderUi();assert(host.routingReady());assert(host.app.renders==2);
 host.closeRouting();assert(!host.route(pending));assert(host.app.routes==1);
 host.reset();assert(!host.routingReady());host.render();
 fui::ActionEvent event;assert(host.routeTouch(input,event)&&event);
 pending={};pending.touchHeld=true;assert(!host.routeTouch(input).routed);assert(host.routeTouch(input,false,true).routed);
 host.resetUi();assert(!host.routeTouch(input,false,true).routed);
}
''')
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I'+td,'-I'+str(ROOT/'src/components'),str(d/'main.cpp'),'-o',str(d/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(d/'probe')],check=True)
        print('PASS: real UiAppHost header/methods, initial/closed/reset gates, repaint readiness, legacy aliases, held routing opt-in (SDK doubles)')
if __name__=='__main__':main()
