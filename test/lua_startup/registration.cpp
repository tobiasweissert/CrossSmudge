#include "startup.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct Heap {
  size_t used=0; bool fail=false;
  static void* alloc(void* ctx,void* p,size_t old,size_t n) {
    auto& h=*static_cast<Heap*>(ctx);if(!p)old=0;
    if(!n){free(p);h.used-=old;return nullptr;}
    if(h.fail || n>76800-(h.used-old))return nullptr;
    void* q=realloc(p,n);if(q)h.used=h.used-old+n;return q;
  }
};
int main(){
  static_assert(sizeof(lua_Integer)==4 && sizeof(lua_Number)==4);
  Heap h;auto* L=lua_newstate(Heap::alloc,&h);assert(L);
  assert(openEssential(L)==LUA_OK);
  assert(openNetwork(L,false)==LUA_OK);
  lua_getglobal(L,"utf8");assert(lua_isnil(L,-1));lua_pop(L,1);
  // Network library loading is tied to manifest declaration, not permission:
  // cached JSON requires the same native UTF8 under an explicit opt-out.
  assert(openNetwork(L,true)==LUA_OK);
  int rc=luaL_dostring(L,"assert(type(utf8)=='table', 'network startup omitted native UTF8'); assert(utf8.char(0xe9,0x1f600)=='é😀'); assert(utf8.len('aé😀')==3); assert(utf8.len('\\255')==nil); assert(io==nil and package==nil and debug==nil)");
  if(rc!=LUA_OK){fprintf(stderr,"%s\n",lua_tostring(L,-1));lua_close(L);return 1;}
  lua_close(L);assert(h.used==0);
  for(int mode=0;mode<2;++mode){
    L=lua_newstate(Heap::alloc,&h);assert(L);
    if(mode)assert(openEssential(L)==LUA_OK);
    h.fail=true;
    rc=mode?openNetwork(L,true):openEssential(L);
    assert(rc==LUA_ERRMEM);h.fail=false;lua_pop(L,1);lua_close(L);assert(h.used==0);
  }
  puts("PASS production native UTF8 registration, offline library set, int32/float32, 75KiB allocator and protected OOM cleanup");
}
