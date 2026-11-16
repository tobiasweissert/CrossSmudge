"""Exercise the actual allocator's combined Browser/API3 pool and reserve policy."""
from pathlib import Path
import subprocess,tempfile,os
R=Path(__file__).resolve().parents[2]
s=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text()
body=s[s.index('void* LuaRunner::customLuaAlloc('):s.index('\nbool LuaRunner::init()')]
program=r'''
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include "BrowserMemory.h"
namespace browser=ink::browser;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_INTERNAL=4;
size_t psram=8*1024*1024,internal=200*1024,capsCalls=0,plainCalls=0;
size_t heap_caps_get_free_size(int caps){return caps&MALLOC_CAP_SPIRAM?psram:internal;}
void* heap_caps_realloc(void* p,size_t n,int){++capsCalls;return std::realloc(p,n);}
void* plain(void* p,size_t n){++plainCalls;return std::realloc(p,n);}
class LuaRunner {public:bool browser_=false,luaHeapUsesPsram_=true;size_t currentAllocatedBytes_=0,maxLuaHeapBytes_=4*1024*1024;static void* customLuaAlloc(void*,void*,size_t,size_t);};
#define realloc plain
BODY
#undef realloc
int main(){
 LuaRunner r;r.browser_=true;psram=browser::PsramReserve;
 assert(!LuaRunner::customLuaAlloc(&r,nullptr,0,16) && "Browser reserve must survive main allocator composition");
 psram=8*1024*1024;internal=browser::InternalEmergencyReserve-1;assert(!LuaRunner::customLuaAlloc(&r,nullptr,0,16));
 internal=200*1024;void* p=LuaRunner::customLuaAlloc(&r,nullptr,0,16);assert(p && capsCalls==1 && plainCalls==0);LuaRunner::customLuaAlloc(&r,p,16,0);
 r.browser_=false;psram=0;p=LuaRunner::customLuaAlloc(&r,nullptr,0,16);assert(p && capsCalls==2 && plainCalls==0 && "Legacy apps retain main's selected PSRAM pool");LuaRunner::customLuaAlloc(&r,p,16,0);
 r.luaHeapUsesPsram_=false;p=LuaRunner::customLuaAlloc(&r,nullptr,0,16);assert(p && plainCalls==1);LuaRunner::customLuaAlloc(&r,p,16,0);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as d:
 p=Path(d);(p/'test.cpp').write_text(program)
 subprocess.run(['c++','-std=c++17','-DARDUINO_ARCH_ESP32','-DBOARD_HAS_PSRAM','-I'+str(R/'lib/LuaBrowser'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS production combined allocator: Browser reserves, API3 selected pool, no fallback')
