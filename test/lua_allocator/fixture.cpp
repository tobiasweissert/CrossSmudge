// Source-extracted production allocator + real firmware Lua, fake heap capabilities.
#include <algorithm>
#include "BrowserMemory.h"
namespace browser=ink::browser;
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_map>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
constexpr uint32_t MALLOC_CAP_SPIRAM=4, MALLOC_CAP_8BIT=8, MALLOC_CAP_INTERNAL=16;
struct Block {size_t size;bool external;};
static std::unordered_map<void*,Block> live;
static bool available=false,fail=false;
static size_t internalUsed=0,externalUsed=0,plainCalls=0,capsCalls=0,externalLimit=8*1024*1024;
static size_t extraInternalPressure=0;
static void fakeFree(void* p) {
 if(!p)return;auto i=live.find(p);assert(i!=live.end());auto old=i->second;
 (old.external?externalUsed:internalUsed)-=old.size;live.erase(i);std::free(p);
}
static void* resize(void* p,size_t n,bool external) {
 if(!n){fakeFree(p);return nullptr;}
 auto it=live.find(p);assert(!p||it!=live.end());Block old=p?it->second:Block{0,false};
 const size_t previous=old.external==external?old.size:0;
 if(fail||(external&&(n>externalLimit||externalUsed-previous>externalLimit-n)))return nullptr;
 void* next=std::malloc(n);if(!next)return nullptr;
 if(p){std::memcpy(next,p,std::min(old.size,n));fakeFree(p);}
 live.emplace(next,Block{n,external});(external?externalUsed:internalUsed)+=n;return next;
}
static void* fakeRealloc(void* p,size_t n) {
 ++plainCalls;
#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && !defined(SIMULATOR)
 return resize(p,n,available&&n>1024);
#else
 return resize(p,n,false);
#endif
}
static void* heap_caps_realloc(void* p,size_t n,uint32_t caps) {
 ++capsCalls;assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));return resize(p,n,true);
}
static size_t heap_caps_get_free_size(uint32_t caps){return caps&MALLOC_CAP_SPIRAM?externalLimit-externalUsed:1024*1024-internalUsed-extraInternalPressure;}
static bool psramHeapAvailable(){return available;}
static struct {uint32_t getFreeHeap(){return 1024*1024-internalUsed-extraInternalPressure;}} ESP;
class LuaRunner {
 public:
 size_t currentAllocatedBytes_=0,maxLuaHeapBytes_=2*1024*1024;
 bool luaHeapUsesPsram_=false, browser_=false;
 static void* customLuaAlloc(void*,void*,size_t,size_t);
 void selectHeap();
};
// run.py injects these exact production source spans, not a rewritten allocator.
#include "production.inc"
static void assertTotals(const LuaRunner& runner) {
 assert(runner.currentAllocatedBytes_==internalUsed+externalUsed);
}
int main(int argc,char**argv) {
 available=argc>1&&std::strcmp(argv[1],"psram")==0;
 LuaRunner runner;runner.selectHeap();
#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && !defined(SIMULATOR)
 assert(runner.luaHeapUsesPsram_==available);
#else
 assert(!runner.luaHeapUsesPsram_);
#endif
 const bool selected=runner.luaHeapUsesPsram_;
 // nullptr osize is a Lua type tag, not allocated bytes.
 void* p=LuaRunner::customLuaAlloc(&runner,nullptr,9,16);assert(p&&live.at(p).external==selected);assertTotals(runner);std::memset(p,0xa5,16);
 p=LuaRunner::customLuaAlloc(&runner,p,16,32);assert(p);for(int i=0;i<16;++i)assert(static_cast<unsigned char*>(p)[i]==0xa5);assertTotals(runner);
 p=LuaRunner::customLuaAlloc(&runner,p,32,8);assert(p);for(int i=0;i<8;++i)assert(static_cast<unsigned char*>(p)[i]==0xa5);assertTotals(runner);
 const size_t beforePlain=plainCalls,beforeCaps=capsCalls;
 fail=true;assert(!LuaRunner::customLuaAlloc(&runner,p,8,64));assert(runner.currentAllocatedBytes_==8&&live.at(p).size==8);for(int i=0;i<8;++i)assert(static_cast<unsigned char*>(p)[i]==0xa5);
 if(selected)assert(plainCalls==beforePlain&&capsCalls==beforeCaps+1);fail=false;
 runner.maxLuaHeapBytes_=12;assert(!LuaRunner::customLuaAlloc(&runner,p,8,16));assert(runner.currentAllocatedBytes_==8);
 assert(!LuaRunner::customLuaAlloc(&runner,p,8,0));assertTotals(runner);assert(live.empty());runner.maxLuaHeapBytes_=2*1024*1024;
 if(selected) {
  // Even with a full PSRAM heap and ample internal room, never fall back.
  externalLimit=0;const auto calls=plainCalls;assert(!LuaRunner::customLuaAlloc(&runner,nullptr,0,32));assert(plainCalls==calls&&internalUsed==0);externalLimit=8*1024*1024;
 }
#if !defined(SIMULATOR) && !defined(BOARD_HAS_PSRAM)
 extraInternalPressure=1024*1024-3000;assert(!LuaRunner::customLuaAlloc(&runner,nullptr,0,16));assert(live.empty());extraInternalPressure=0;
#endif
 // All allocations from a real Lua state pass through the extracted body.
 lua_State* L=lua_newstate(LuaRunner::customLuaAlloc,&runner);assert(L);
 luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);luaL_requiref(L,"string",luaopen_string,1);lua_pop(L,1);
 assert(luaL_dostring(L,"t={};for i=1,600 do t[i]={i,tostring(i),x=string.rep('x',31)} end")==LUA_OK);assertTotals(runner);
 if(selected)assert(internalUsed==0&&externalUsed>0);else assert(internalUsed>0);
 // Force the actual VM through emergency-GC/allocation failure, then recover.
 runner.maxLuaHeapBytes_=runner.currentAllocatedBytes_+4096;
 int rc=luaL_loadstring(L,"for i=1,100000 do t[#t+1]={i,i+1,i+2} end");if(rc==LUA_OK)rc=lua_pcall(L,0,0,0);assert(rc==LUA_ERRMEM);lua_pop(L,1);assertTotals(runner);
 runner.maxLuaHeapBytes_=2*1024*1024;
 assert(luaL_dostring(L,"t=nil;collectgarbage('collect');x=42;assert(x==42)")==LUA_OK);
 lua_close(L);assert(runner.currentAllocatedBytes_==0&&live.empty()&&internalUsed==0&&externalUsed==0);
 if(selected)assert(plainCalls==0);else assert(capsCalls==0);
 std::cout<<"allocator policy, grow/shrink, payload/failure preservation, cap, real-Lua OOM/recovery/close: PASS; PSRAM="<<selected<<"\n";
}
