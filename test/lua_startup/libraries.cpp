#include "LuaRuntime.h"
#include "baseline.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct Heap {
 size_t used=0,peak=0; bool fail=false;
 static void* alloc(void* ctx,void* p,size_t old,size_t n){
  auto& h=*static_cast<Heap*>(ctx);if(!p)old=0;
  if(!n){free(p);h.used-=old;return nullptr;}
  if(h.fail || n>76800-(h.used-old))return nullptr;
  void* q=realloc(p,n);if(q){h.used=h.used-old+n;if(h.used>h.peak)h.peak=h.used;}return q;
 }
};
int main(){
 static_assert(sizeof(lua_Integer)==4 && sizeof(lua_Number)==4);
 size_t baseline=0;
 for(int mode=0;mode<2;++mode){
  Heap h;auto* L=lua_newstate(Heap::alloc,&h);assert(L);lua_gc(L,LUA_GCINC,105,250,0);
  if(mode==0) baselineEssential(L);else assert(ink::runtime::openEssential(L)==LUA_OK);
  lua_gc(L,LUA_GCCOLLECT);
  if(!mode)baseline=h.used;
  else {printf("Offline retained baseline %zu, current %zu (delta %zd)\n",baseline,h.used,static_cast<ptrdiff_t>(h.used)-baseline);assert(h.used==baseline);}
  size_t before=h.used;assert(ink::runtime::openNetwork(L,false)==LUA_OK);assert(h.used==before);
  lua_getglobal(L,"utf8");assert(lua_isnil(L,-1));lua_pop(L,1);
  if(mode){
   assert(ink::runtime::openNetwork(L,true)==LUA_OK);
   lua_gc(L,LUA_GCCOLLECT);printf("Native UTF8 retained increment %zu bytes\n",h.used-before);
   assert(luaL_dostring(L,"assert(utf8.len('aé')==2); assert(utf8.char(0x1f600)=='😀'); assert(utf8.len('\\255')==nil); assert(io==nil and package==nil and debug==nil)")==LUA_OK);
  }
  lua_close(L);assert(h.used==0);
 }
 for(int mode=0;mode<2;++mode){
  Heap h;auto* L=lua_newstate(Heap::alloc,&h);assert(L);
  if(mode)assert(ink::runtime::openEssential(L)==LUA_OK);
  h.fail=true;
  int result=mode?ink::runtime::openNetwork(L,true):ink::runtime::openEssential(L);
  assert(result==LUA_ERRMEM);h.fail=false;lua_pop(L,1);lua_close(L);assert(h.used==0);
 }
 puts("PASS native libraries, int32/float32, offline baseline, startup OOM cleanup");
}
