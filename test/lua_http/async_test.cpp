#include "LuaHttp.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>
extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}
using namespace ink::http;
static std::atomic<bool> respond{false};
static void run(void*, Job& j) {
  if(respond.load()){j.committed=j.download;j.result.status=200;j.result.size=8192;memset(j.result.body,'x',8192);return;}
  while(!j.cancel.load()) std::this_thread::yield();
  j.result.error="cancelled";
}
struct LuaHeap {
 size_t used=0;bool fail=false;
 static void* alloc(void* context,void* p,size_t oldSize,size_t newSize) {
   auto& h=*static_cast<LuaHeap*>(context);if(!p)oldSize=0;
   if(!newSize){free(p);h.used-=oldSize;return nullptr;}
   if(h.fail || newSize>75*1024-(h.used-oldSize))return nullptr;
   void* result=realloc(p,newSize);if(result)h.used=h.used-oldSize+newSize;return result;
 }
};
static bool spawn(void(*f)(void*),void* p) { std::thread(f,p).detach();return true; }
static void script(lua_State* L,const char* s){assert(luaL_dostring(L,s)==LUA_OK);}
int main(){
  LuaHeap heap;lua_State* L=lua_newstate(LuaHeap::alloc,&heap);luaL_openlibs(L);
  restrictStandardLibraries(L);
  script(L,"assert(os.execute==nil and os.getenv==nil and loadfile==nil); assert(load('return 7'))(); assert(load(string.dump(function() end))==nil)");
  Session s({nullptr,run,spawn});assert(s.domains.add("example.com"));
  lua_newtable(L);registerApi(L,s);lua_setglobal(L,"smudge");
  script(L,"local id,e=smudge.http_request{url='https://example.com/'}; assert(id==nil and e=='permission_denied')");
  s.allowed=true;
  script(L,"local opts=setmetatable({url='https://example.com/',method='bad'}, {__index=function() error('injected') end}); local ok=pcall(smudge.http_request,opts); assert(ok)");
  s.close();s.allowed=true;
  script(L,"local id,e=smudge.http_request{url='https://other.com/'}; assert(id==nil and e=='domain_denied')");
  script(L,"local id,e=smudge.http_request{url='https://example.com/', headers={Authorization='secret'}}; assert(id==nil and e=='invalid_headers')");
  script(L,"local id,e=smudge.http_request{url='https://example.com/\\0evil'}; assert(id==nil and e=='invalid_url')");
  script(L,"id=assert(smudge.http_request{url='https://example.com/', method='POST',body='a\\0b'}); local x,e=smudge.http_request{url='https://example.com/'}; assert(x==nil and e=='busy'); assert(smudge.http_cancel(id)); function on_http(e) assert(e.id==id and e.error=='cancelled'); got=true end");
  for(int i=0;i<1000 && !s.ready();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(s.ready());assert(dispatch(L,s)==LUA_OK);script(L,"assert(got)");
  script(L,"assert(smudge.http_request{url='https://example.com/'})");s.close();
  for(int i=0;i<1000 && Session::busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(!Session::busy());respond.store(true);s.allowed=true;
  script(L,"assert(smudge.http_request{url='https://example.com/'})");
  for(int i=0;i<1000 && !s.ready();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(s.ready());heap.fail=true;assert(dispatch(L,s)==LUA_ERRMEM);heap.fail=false;lua_pop(L,1);
  s.close();lua_close(L);assert(heap.used==0);
  for(int i=0;i<1000 && Session::busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(!Session::busy());
  // API4 registration uses the same slot/cancellation/drainage and protected OOM path.
  L=lua_newstate(LuaHeap::alloc,&heap);luaL_openlibs(L);
  Session files({nullptr,run,spawn,[](const char*,size_t,char* p,size_t n,size_t& got,size_t& total)->const char* {
    memset(p,'z',n);got=n;total=2048;return nullptr;
  }});
  files.fileApi=true;files.allowed=true;assert(files.domains.add("example.com"));assert(files.configureFileRoot("/.smudge/applications/demo"));
  lua_newtable(L);registerApi(L,files);lua_setglobal(L,"smudge");
  respond=false;
  script(L,"id=assert(smudge.http_download{url='https://example.com/',max_bytes=8192});local x,e=smudge.download_read(0,1);assert(not x and e=='busy');assert(smudge.http_cancel(id));function on_http(e) assert(e.kind=='download' and e.error=='cancelled' and e.body==nil and e.size==nil);local x,err=smudge.download_read(0,1);assert(not x and err=='busy') end");
  for(int i=0;i<1000 && !files.ready();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(files.ready());assert(dispatch(L,files)==LUA_OK);
  script(L,"assert(smudge.http_download{url='https://example.com/',max_bytes=8192})");files.close();
  for(int i=0;i<1000 && Session::busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(!Session::busy());assert(dispatch(L,files)==LUA_OK);
  files.fileApi=true;files.allowed=true;assert(files.configureFileRoot("/apps/demo"));respond=true;
  script(L,"assert(smudge.http_download{url='https://example.com/',max_bytes=8192})");
  for(int i=0;i<1000 && !files.ready();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(files.ready());heap.fail=true;assert(dispatch(L,files)==LUA_ERRMEM);heap.fail=false;lua_pop(L,1);
  assert(luaL_loadstring(L,"return smudge.download_read(0,1024)")==LUA_OK);
  heap.fail=true;assert(lua_pcall(L,0,2,0)==LUA_ERRMEM);heap.fail=false;lua_pop(L,1);
  script(L,"local s,n=smudge.download_read(0,1024);assert(#s==1024 and n==2048)");
  files.close();lua_close(L);assert(heap.used==0);

}
