def emit(root, out):
    text=(root/'src/activities/reader/ReaderUtils.h').read_text()
    start=text.index('struct SideShortcutState')
    end=text.index('// END independent side shortcut routing',start)
    code='''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
struct CrossPointSettings {enum {IGNORE=0}; uint8_t sideButtonUpShort=31,sideButtonDownShort=2,sideButtonUpLong=64,sideButtonDownLong=65;};
inline CrossPointSettings SETTINGS;
struct MappedInputManager {
 enum class Button {Up,Down,Power};
 bool down[3]{},press[3]{},release[3]{}; unsigned long held=0;
 bool isPressed(Button b) const{return down[(int)b];}
 bool isPhysicalPressed(Button b) const{return down[(int)b];}
 bool wasPressed(Button b) const{return press[(int)b];}
 bool wasReleased(Button b) const{return release[(int)b];}
 unsigned long getHeldTime() const {return held;}
};
constexpr unsigned long SKIP_HOLD_MS=700;
'''+text[start:end]+'''
int main() {
 SideShortcutState s; MappedInputManager i; std::vector<int> actions;
 auto send=[&](uint8_t a){actions.push_back(a);return true;};
 i.release[0]=true; assert(routeSideShortcuts(i,s,send)); assert(actions.empty()); // queued release is not fresh
 i={};i.down[0]=true;i.press[0]=true;assert(routeSideShortcuts(i,s,send));assert(actions.empty());
 i.press[0]=false;i.held=701;assert(routeSideShortcuts(i,s,send));assert(actions.size()==1&&actions.back()==64);
 i.held=1200;routeSideShortcuts(i,s,send);assert(actions.size()==1);
 i={};i.release[0]=true;routeSideShortcuts(i,s,send);assert(actions.size()==1);
 i={};routeSideShortcuts(i,s,send);
 i.down[1]=true;i.press[1]=true;routeSideShortcuts(i,s,send);
 i={};i.release[1]=true;i.held=100;routeSideShortcuts(i,s,send);assert(actions.size()==2&&actions.back()==2);
 // Independent settings: Down hold disabled makes short action immediate only once.
 i={};routeSideShortcuts(i,s,send);SETTINGS.sideButtonDownLong=0;SETTINGS.sideButtonDownShort=33;
 i.down[1]=true;i.press[1]=true;routeSideShortcuts(i,s,send);assert(actions.size()==3&&actions.back()==33);
 i={};i.release[1]=true;routeSideShortcuts(i,s,send);assert(actions.size()==3);
 // A power chord cannot leak an Up short after Power releases first.
 i={};i.down[2]=true;i.down[0]=true;i.press[0]=true;routeSideShortcuts(i,s,send);
 i={};i.down[0]=true;routeSideShortcuts(i,s,send);
 i={};i.release[0]=true;routeSideShortcuts(i,s,send);assert(actions.size()==3);
 puts("PASS production independent side short/hold, hold-once, stale-release, chord quarantine, Library binding");
}
'''
    (out/'activities.cpp').write_text(code)
