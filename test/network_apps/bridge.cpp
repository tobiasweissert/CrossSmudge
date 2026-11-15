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
static int calls,metadataCalls;
static void run(void*,Job& j) {
  ++calls;
  const char* body = !strcmp(j.request.url.host,"opentdb.com") ?
    "{\"response_code\":0,\"results\":[{\"type\":\"multiple\",\"question\":\"SYNTHETIC\",\"correct_answer\":\"Yes\",\"incorrect_answers\":[\"No\",\"Maybe\",\"Never\"]}]}" :
    strstr(j.request.url.path,"topstories") ? "[11,22,33,44,55,66]" :
    "{\"id\":11,\"type\":\"story\",\"title\":\"SYNTHETIC bridge story\",\"url\":\"https://example.com/\",\"text\":\"Hello\"}";
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
  std::string identity;
  if(!strcmp(j.request.url.host,"hacker-news.firebaseio.com") && strstr(j.request.url.path,"item/")) {
    ++metadataCalls;
    if(metadataCalls==2){j.result.error="connect";return;}
    const char* number=strstr(j.request.url.path,"item/")+5;
    identity=body;auto at=identity.find("\"id\":11");
    if(at!=std::string::npos)identity.replace(at,7,"\"id\":"+std::to_string(atoi(number)));
    if(metadataCalls==6){
      const std::string link=",\"url\":\"https://example.com/\"";
      auto url=identity.find(link);if(url!=std::string::npos)identity.erase(url,link.size());
    }
    body=identity.c_str();
  }
  j.result.status=200;j.result.size=strlen(body);assert(j.result.size<=MaxBody);memcpy(j.result.body,body,j.result.size);
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
  if(!strcmp(argv[1],"cp-hackernews")) {
    script(L,"reader=dofile('main.lua'); on_init(); on_draw(); on_update()");
    for(int i=0;i<7 && s.ready();++i) {
      assert(dispatch(L,s)==LUA_OK);script(L,"on_update(); on_draw()");
    }
    script(L,"assert(reader.state().items[1].id==11 and reader.state().failed[2] and reader.state().items[6]); function tap(kind,id) on_draw(); for _,b in ipairs(reader.view.hit) do if b.kind==kind and (not id or b.id==id) then on_touch(kind=='story' and b.x+100 or b.x+b.w//2,b.y+b.h//2,'tap'); return end end; error('missing hit '..kind) end; tap('comments',6); assert(reader.state().screen=='comments'); on_button('back'); tap('story',6); assert(reader.state().screen=='article' and reader.state().articleText:sub(1,5)=='Hello'); on_button('back'); tap('retry',2); on_update()");
    assert(s.ready());assert(dispatch(L,s)==LUA_OK);
    script(L,"on_update(); assert(reader.state().items[2] and not reader.state().failed[2]); tap('story',1); assert(reader.state().screen=='article'); on_update()");
    assert(s.ready());
    script(L,"on_button('back'); assert(reader.state().screen=='list')");
    assert(dispatch(L,s)==LUA_OK); // stale cancelled article must not reopen itself.
    script(L,"assert(reader.state().screen=='list'); tap('story',1); on_update()");
    assert(s.ready());assert(dispatch(L,s)==LUA_OK);
    script(L,"on_draw(); assert(reader.state().screen=='article' and #reader.state().articleText==8192)");
  } else {
    script(L,"dofile('main.lua'); on_init(); on_draw(); on_button('confirm'); on_update()");
    assert(calls==1 && s.ready());assert(dispatch(L,s)==LUA_OK);assert(!Session::busy());
    script(L,"on_update(); on_draw(); on_button('confirm'); on_draw(); assert(rendered:find('SYNTHETIC'))");
  }
  script(L,"on_exit(); on_update()");s.close();lua_close(L);assert(h.used==0);assert(!Session::busy());
  printf("PASS production bridge %s: peak %zu/%zu; retained %zu\n",argv[1],h.peak,h.limit,h.used);
}
