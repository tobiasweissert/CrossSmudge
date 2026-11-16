// Production HTTP registration + actual shipped Lua apps. Synthetic native
// transport only: this does not claim DNS/TLS/device/network verification.
#include "LuaHttp.h"
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
  const char* body = !strcmp(j.request.url.host,"opentdb.com") ?
    "{\"response_code\":0,\"results\":[{\"type\":\"multiple\",\"question\":\"SYNTHETIC\",\"correct_answer\":\"Yes\",\"incorrect_answers\":[\"No\",\"Maybe\",\"Never\"]}]}" :
    strstr(j.request.url.path,"topstories") ? "[11,22]" :
    "{\"id\":11,\"type\":\"story\",\"title\":\"SYNTHETIC bridge story\",\"text\":\"Hello\"}";
  std::string large;
  if(getenv("CP_STRESS") && !strcmp(j.request.url.host,"hacker-news.firebaseio.com")) {
    if(strstr(j.request.url.path,"topstories")) {
      large="[11";for(int i=12;i<511;++i)large+=","+std::to_string(i);large+="]";
    } else large="{\"id\":11,\"type\":\"story\",\"title\":\""+std::string(1024,'t')+"\",\"url\":\"https://example.com/\",\"text\":\"Hello "+std::string(6000,'x')+"\"}";
    body=large.c_str();
  }
  if(getenv("CP_STRESS") && !strcmp(j.request.url.host,"opentdb.com")) {
    large="{\"response_code\":0,\"results\":[{\"type\":\"multiple\",\"question\":\"SYNTHETIC "+std::string(1500,'q')+"\",\"correct_answer\":\"Yes"+std::string(1500,'a')+"\",\"incorrect_answers\":[\"No"+std::string(1500,'b')+"\",\"Maybe"+std::string(1500,'c')+"\",\"Never"+std::string(1500,'d')+"\"]}]}";
    body=large.c_str();
  }
  if(!strcmp(j.request.url.host,"r.jina.ai")){large=std::string(8192,'a');body=large.c_str();}
  j.result.status=200;j.result.size=strlen(body);memcpy(j.result.body,body,j.result.size);
}
static bool spawn(void(*f)(void*),void* p){f(p);return true;}
static void script(lua_State* L,const char* s) {
  if(luaL_dostring(L,s)!=LUA_OK){fprintf(stderr,"%s\n",lua_tostring(L,-1));exit(1);}
}
int main(int argc,char** argv) {
  assert(argc==2);
  Heap h; if(const char* limit=getenv("LUA_APP_LIMIT"))h.limit=strtoul(limit,nullptr,10);
  lua_State* L=lua_newstate(Heap::alloc,&h); assert(L);luaL_openlibs(L);
  lua_pushstring(L,argv[1]);lua_setglobal(L,"APP");
  if(luaL_dofile(L,"test/network_apps/bridge.lua")!=LUA_OK){fprintf(stderr,"%s\n",lua_tostring(L,-1));return 1;}
  restrictStandardLibraries(L);
  Session s({nullptr,run,spawn});s.allowed=true;
  if(!strcmp(argv[1],"cp-hackernews")) {
    assert(s.domains.add("hacker-news.firebaseio.com"));assert(s.domains.add("r.jina.ai"));
  } else assert(s.domains.add("opentdb.com"));
  std::string resolved;
  assert(!resolveAppPath("../other/main.lua","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  assert(!resolveAppPath("/apps/other/main.lua","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  assert(resolveAppPath("feed1.cache","/apps/cp-hackernews","/saves/cp-hackernews",resolved));
  lua_getglobal(L,"smudge");registerApi(L,s);lua_pop(L,1);
  script(L,"dofile('main.lua'); on_init(); on_draw(); if APP=='cp-hackernews' then on_button('up') end; on_button('confirm'); on_update()");
  assert(calls==1 && s.ready()); assert(dispatch(L,s)==LUA_OK); assert(!Session::busy());
  script(L,"on_update(); on_draw()");
  if(!strcmp(argv[1],"cp-hackernews")) {if(calls!=2 || !s.ready()) {
    fprintf(stderr,"FAIL: item chain did not complete at Lua cap %zu (peak %zu)\n",h.limit,h.peak);
    s.close();lua_close(L);return 1;
  }assert(dispatch(L,s)==LUA_OK);script(L,"on_button('down'); on_button('confirm'); on_draw()");}
  else script(L,"on_button('confirm'); on_draw(); assert(rendered:find('SYNTHETIC'))");
  if(getenv("CP_STRESS") && !strcmp(argv[1],"cp-hackernews")) {
    script(L,"on_button('back'); on_button('down'); on_button('confirm'); on_update()");
    assert(s.ready()); assert(dispatch(L,s)==LUA_OK);script(L,"on_draw(); assert(rendered:find('saved for offline'))");
  }
  script(L,"on_exit(); on_update()");s.close();lua_close(L);assert(h.used==0);assert(!Session::busy());
  printf("PASS production bridge %s: peak %zu/%zu; retained %zu\n",argv[1],h.peak,h.limit,h.used);
}
