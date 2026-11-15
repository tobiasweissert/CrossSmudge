"""Compile production input classifier, MappedInput mapping, dispatch and Lua callback.
Hardware snapshots/clock and logical coordinate transform are host fixtures.
No SDK edits; production bodies are extracted to prevent a second implementation.
"""
import pathlib,re,subprocess,tempfile,shutil,json,sys
R=pathlib.Path(__file__).resolve().parents[2];T=R/'test/minesweeper_graphics';O=pathlib.Path('/root/github/.host-data/crosssmudge/minesweeper-graphics-hold')
def body(s,signature):
 start=s.index(signature); brace=s.index('{',start);depth=1;end=brace+1
 while depth:
  if s[end]=='{':depth+=1
  elif s[end]=='}':depth-=1
  end+=1
 return s[start:end]
sdk=(R/'freeink-sdk/libs/hardware/InputManager/src/InputManager.cpp').read_text();mapped=(R/'src/MappedInputManager.cpp').read_text();runner=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text()
classifier=body(sdk,'  if (touchPressed && !touchMultiContactSequence && !touchMovedBeyondTapSlop && !touchLongPressFired &&')
cpp='''
#include <cstdio>
#include <cstring>
#include <cmath>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#define FREEINK_CAP_TOUCH 1
#define FREEINK_DEVICE_STICKY 1
#define CROSSINK_APP_DEVICE_STICKY 1
#define TOUCH_LONG_PRESS_MS 500
unsigned long now=0;
unsigned long millis(){return now;}
void xQueueReset(void*){}
struct Point{float x=0,y=0;unsigned long timestamp=0;};
struct InputManager {
 bool touchPressed=false,touchReleasedEvent=false,touchSuppressed=false,touchMultiContactSequence=false;
 bool touchMovedBeyondTapReleaseSlop=false,touchMovedBeyondTapSlop=false,touchLongPressFired=false,touchLongPressEvent=false;
 Point touchDownPoint;void* _asyncMultiTouchSwipeQueue=nullptr;void* _asyncMultiTouchRotationQueue=nullptr;void* _asyncMultiTouchPinchQueue=nullptr;
 void normalizeTouchPoint(float x,float y,float& nx,float& ny)const{nx=x/480;ny=y/800;}
 void cancelMultiTouchGesture(){}
 unsigned long lastTouchHeldMs()const{return now-touchDownPoint.timestamp;}
 bool wasTouchActivity()const{return touchReleasedEvent;}
 bool wasTouchTap(float&,float&)const;bool wasTouchLongPress(float&,float&)const;void suppressTouchContact();
 void classify(){CLASSIFIER}
};
struct Renderer {void tapToLogical(float nx,float ny,int& x,int& y)const{x=std::lround(nx*480);y=std::lround(ny*800);}};
struct MappedInputManager {
 InputManager& gpio;Renderer renderer;bool enabled=true;mutable bool suppressTouchTap=false;
 bool touchInputEnabled()const{return enabled;}mutable bool touchHeldOverrideValid=false;mutable unsigned long touchHeldOverrideMs=0,touchHeldOverrideAt=0;void rememberTouchHeldTime()const;
 bool wasScreenTapped(int&,int&,unsigned long&)const;bool wasScreenTapped(int&,int&)const;bool wasScreenLongPress(int&,int&)const;
};
class LuaRunner {public:lua_State* L=nullptr;bool hasError_=false;static LuaRunner* s_activeRunner;
 void setError(const char* e){hasError_=true;fprintf(stderr,"%s\\n",e);}
 bool hasLongPressHandler();void onLongPress(int,int);void onTap(int,int);void onTouch(int,int,const char*);
 static int l_getDevice(lua_State*);
};
LuaRunner* LuaRunner::s_activeRunner=nullptr;
'''.replace('CLASSIFIER',classifier)
for signature in ('bool InputManager::wasTouchTap(', 'bool InputManager::wasTouchLongPress(', 'void InputManager::suppressTouchContact('):cpp+=body(sdk,signature)+'\n'
for signature in ('bool MappedInputManager::wasScreenTapped(int& x, int& y) const','bool MappedInputManager::wasScreenLongPress(', 'void MappedInputManager::rememberTouchHeldTime(', 'bool MappedInputManager::wasScreenTapped(int& x, int& y, unsigned long& heldMs) const'):cpp+=body(mapped,signature)+'\n'
for signature in ('bool LuaRunner::hasLongPressHandler(', 'void LuaRunner::onLongPress(', 'void LuaRunner::onTap(', 'void LuaRunner::onTouch(', 'int LuaRunner::l_getDevice('):cpp+=body(runner,signature)+'\n'
cpp+='''
#include "LuaTouchDispatch.h"
static InputManager gpio;static MappedInputManager input{gpio};
extern "C" int production_get_device(lua_State* L){return LuaRunner::l_getDevice(L);}
extern "C" int input_frame(lua_State* L){
 const char* action=luaL_checkstring(L,1);now=luaL_optinteger(L,4,now);
 gpio.touchReleasedEvent=false;gpio.touchLongPressEvent=false;
 if(!strcmp(action,"down")){gpio=InputManager{};gpio.touchPressed=true;gpio.touchDownPoint={float(luaL_checkinteger(L,2)),float(luaL_checkinteger(L,3)),now};}
 if(!strcmp(action,"move")){gpio.touchMovedBeyondTapSlop=true;gpio.touchMovedBeyondTapReleaseSlop=true;}
 if(!strcmp(action,"roll"))gpio.touchMovedBeyondTapSlop=true;
 if(!strcmp(action,"cancel"))gpio.suppressTouchContact();
 if(!strcmp(action,"multi"))gpio.touchMultiContactSequence=true;
 if(!strcmp(action,"release")){gpio.touchPressed=false;gpio.touchReleasedEvent=true;}
 input.enabled=strcmp(action,"disabled")!=0;
 gpio.classify();LuaRunner runner;runner.L=L;
 bool handled=dispatchLuaTouch(input,runner);
 lua_pushboolean(L,handled);lua_pushboolean(L,gpio.touchSuppressed);return 2;
}
'''
with tempfile.TemporaryDirectory(dir='/tmp',prefix='mine-input-') as tmp:
 p=pathlib.Path(tmp)
 if '--legacy-dispatch' in sys.argv:
  (p/'LuaTouchDispatch.h').write_text('template<class I,class R> bool dispatchLuaTouch(I& i,R& r){int x=0,y=0;if(i.wasScreenTapped(x,y)){r.onTap(x,y);r.onTouch(x,y,"tap");return true;}return false;}')
 (p/'input.cpp').write_text(cpp);inc='-I'+str(R/'lib/Lua/src')
 subprocess.run(['g++','-std=c++17',inc,'-I'+str(R/'src/activities/apps/lua'),'-c',str(p/'input.cpp'),'-o',str(p/'input.o')],check=True)
 s=(R/'test/crossplay_ports/runner.c').read_text().replace('static const luaL_Reg api[]=', 'extern int input_frame(lua_State*);extern int production_get_device(lua_State*);\nstatic const luaL_Reg api[]=')
 s=s.replace('{"get_bounds",bounds}', '{"get_device",production_get_device},{"bitmap",draw},{"rect_dither",draw},{"get_bounds",bounds}')
 s=s.replace(' int err=luaL_loadfile',' lua_pushcfunction(L,input_frame);lua_setglobal(L,"input_frame");\n int err=luaL_loadfile');(p/'runner.c').write_text(s)
 subprocess.run(['cc','-O2',inc,str(p/'runner.c'),*map(str,(R/'lib/Lua/src').glob('*.c')),str(p/'input.o'),'-lm','-lstdc++','-o',str(p/'runner')],check=True)
 for cap in (76800,2097152):
  app=p/str(cap);shutil.copytree(R/'apps/cp-minesweeper',app)
  shutil.copyfile(T/'input_cases.lua',app/'input_cases.lua')
  script='input_capacity.lua' if cap==76800 else 'input.lua'
  subprocess.run([str(p/'runner'),str(T/script),str(cap)],cwd=app,check=True)
print('Production classifier -> SDK query/suppression -> MappedInput -> opt-in dispatcher -> real Lua callback PASS')
