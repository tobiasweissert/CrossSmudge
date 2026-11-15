// Exact production library initialization and HTTP registration, shipped HN app.
// Captured public bytes through a native transport double; no device TLS claim.
#include "LuaHttp.h"
#include "startup.h"
#include "HttpManifest.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}
using namespace ink::http;
struct Heap {
  size_t used=0,peak=0,limit=2*1024*1024;
  static void* alloc(void* c,void* p,size_t old,size_t n) {
    auto& h=*static_cast<Heap*>(c); if(!p)old=0;
    if(!n){free(p);h.used-=old;return nullptr;}
    if(n>h.limit-(h.used-old))return nullptr;
    void* q=realloc(p,n);if(q){h.used=h.used-old+n;if(h.used>h.peak)h.peak=h.used;}return q;
  }
};
static int calls;
static void run(void*,Job& j) {
  ++calls;
  if (getenv("HTTP_ERROR")) { j.result.error=getenv("HTTP_ERROR"); return; }
  const char* kind = strstr(j.request.url.path,"topstories") ? "feed" : "item";
  std::ifstream input(std::string(getenv("CP_FIXTURES"))+"/"+kind+".body");
  assert(input); std::string captured((std::istreambuf_iterator<char>(input)),{});
  const char* body=captured.c_str();
  j.result.status=200;j.result.size=strlen(body);memcpy(j.result.body,body,j.result.size);
}
static bool spawn(void(*f)(void*),void* p){f(p);return true;}
static void script(lua_State* L,const char* s) {
  if(luaL_dostring(L,s)!=LUA_OK){fprintf(stderr,"%s\n",lua_tostring(L,-1));exit(1);}
}
int main(int argc,char** argv) {
  assert(argc==2);
  Heap h; if(const char* limit=getenv("LUA_APP_LIMIT"))h.limit=strtoul(limit,nullptr,10);
  lua_State* L=lua_newstate(Heap::alloc,&h); assert(L);
  lua_gc(L,LUA_GCINC,105,250,0);
  assert(openEssential(L)==LUA_OK);
  std::ifstream manifest(std::string("apps/")+argv[1]+"/manifest.json");
  std::string raw((std::istreambuf_iterator<char>(manifest)),{});
  JsonDocument doc; assert(!deserializeJson(doc,raw));
  Domains domains; assert(readNetworkDeclaration(doc["network"],domains));
  assert(openNetwork(L,domains.count!=0)==LUA_OK);
  lua_pushstring(L,argv[1]);lua_setglobal(L,"APP");
  if(luaL_dofile(L,"test/lua_startup/host.lua")!=LUA_OK){fprintf(stderr,"%s\n",lua_tostring(L,-1));return 1;}
  restrictStandardLibraries(L);
  Session s({nullptr,run,spawn});s.allowed=true;
  s.domains=domains;
  s.allowed=getenv("OPT_OUT")==nullptr;
  std::string resolved;
  assert(!resolveAppPath("../other/main.lua","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  assert(!resolveAppPath("/apps/other/main.lua","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  assert(resolveAppPath("feed1.cache","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  lua_getglobal(L,"smudge");registerApi(L,s);lua_pop(L,1);
  script(L,"dofile('main.lua'); on_init(); on_draw(); on_button('up'); on_button('confirm'); on_update()");
  if (getenv("OPT_OUT")) {
    assert(calls==0 && !s.ready());
    script(L,"on_draw(); assert(rendered:find('permission_denied')); on_exit()");
  } else {
    assert(calls==1 && s.ready());
    if (getenv("CANCEL")) script(L,"on_button('back')");
    int result=dispatch(L,s);
    if (getenv("EXPECT_OOM")) {
      assert(result==LUA_ERRMEM);
      fprintf(stdout,"PASS bounded real-feed OOM: %s; peak %zu/%zu\n",lua_tostring(L,-1),h.peak,h.limit);
      lua_pop(L,1);
    } else {
      if(result!=LUA_OK){fprintf(stderr,"dispatch: %s\n",lua_tostring(L,-1));return 1;}
      script(L,"on_update(); on_draw()");
      if(getenv("HTTP_ERROR")) {
        lua_pushstring(L,getenv("HTTP_ERROR"));lua_setglobal(L,"EXPECTED_ERROR");
        script(L,"assert(rendered:find(EXPECTED_ERROR=='time_untrusted' and 'Trust verified UTC' or EXPECTED_ERROR)); assert(next(TEST_FILES)==nil)");
        assert(calls==1);
      } else if(getenv("CANCEL")) {
        script(L,"assert(rendered:find('Cancelled')); assert(next(TEST_FILES)==nil)");
        assert(calls==1);
      } else {
        assert(calls==2 && s.ready());
        result=dispatch(L,s);
        if(result!=LUA_OK){fprintf(stderr,"item dispatch: %s\n",lua_tostring(L,-1));return 1;}
        std::ifstream input(std::string(getenv("CP_FIXTURES"))+"/item.body");
        std::string item((std::istreambuf_iterator<char>(input)),{});
        lua_pushlstring(L,item.data(),item.size());lua_setglobal(L,"EXPECTED_BODY");
        script(L,"local M=dofile('model.lua'); local C=dofile('cache.lua'); assert(C.open('story',M.item).body==EXPECTED_BODY); on_draw(); assert(rendered:find(M.plain(assert(M.item(EXPECTED_BODY)).title):sub(1,20),1,true)); on_exit(); on_init(); on_draw(); assert(rendered:find('Offline cache')); on_exit()");
      }
      script(L,"on_exit(); on_update()");
    }
  }
  s.close();lua_close(L);assert(h.used==0);assert(!Session::busy());
  printf("PASS production startup HN: peak %zu/%zu; retained %zu\n",h.peak,h.limit,h.used);
}
