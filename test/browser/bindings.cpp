#include "LuaRuntime.h"
#include "LuaHttp.h"
#include "HttpManifest.h"
#include "LuaFileRead.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
using namespace ink::http;
struct Heap{size_t used=0,limit=4*1024*1024;static void* alloc(void* c,void* p,size_t old,size_t n){auto& h=*(Heap*)c;if(!p)old=0;if(!n){free(p);h.used-=old;return nullptr;}if(n>h.limit-(h.used-old))return nullptr;void* q=realloc(p,n);if(q)h.used=h.used-old+n;return q;}};
static size_t response=12000;
static void run(void*,Job& j){assert(j.request.browser);j.result.status=200;j.result.size=response;std::string block(2048,'x');for(size_t n=0;n<response;n+=block.size())assert(j.resource.append(block.data(),std::min(block.size(),response-n)));strcpy(j.result.url,"https://example.org/final");strcpy(j.result.contentType,"text/html");}
static bool spawn(void(*f)(void*),void* p){f(p);return true;}
static void(*later)(void*);static void* laterArgument;
static bool defer(void(*fn)(void*),void* p){later=fn;laterArgument=p;return true;}
static void script(lua_State* L,const char* code){if(luaL_dostring(L,code)){fprintf(stderr,"%s\n",lua_tostring(L,-1));std::abort();}}
int main(){
 JsonDocument manifest;assert(!deserializeJson(manifest,"{\"public_https\":true}"));Domains domains;
 assert(!readNetworkDeclaration(manifest.as<JsonVariantConst>(),domains));domains.publicHttps=true;
 std::string directory;assert(resolveAppDirectory("/saves/browser","/apps/browser","/saves/browser",directory));assert(!resolveAppDirectory("/saves/other","/apps/browser","/saves/browser",directory));
 Heap h;lua_State* L=lua_newstate(Heap::alloc,&h);assert(L);assert(ink::runtime::openEssential(L)==LUA_OK);assert(ink::runtime::openNetwork(L,true)==LUA_OK);
 Session s({nullptr,run,spawn});s.allowed=true;s.domains=domains;std::filesystem::create_directories(".pio/browser-binding-data");strcpy(s.resourceRoot,".pio/browser-binding-data");lua_newtable(L);registerApi(L,s);lua_setglobal(L,"smudge");restrictStandardLibraries(L);
 script(L,"old={text='Previous useful page'}; function on_http(e) event=e end; assert(smudge.http_request{url='https://example.org/one'})");assert(dispatch(L,s)==LUA_OK);
 script(L,"assert(event.handle and not event.body and #smudge.http_read(event.handle,0,8192)==8192 and #smudge.http_read(event.handle,8192,8192)==3808);assert(not smudge.http_read(event.handle,0,8193));assert(smudge.http_release(event.handle));local id,e=smudge.http_request{url='https://example.org/',method='POST'};assert(not id and e=='invalid_method')");
 response=BrowserBody;script(L,"function on_http(e) if e.handle then smudge.http_read(e.handle,0,8192) end; event=e end;event=nil;collectgarbage('collect');assert(smudge.http_request{url='https://another.example.org/'})");
 h.limit=h.used+2048;assert(dispatch(L,s)==LUA_OK);h.limit=4*1024*1024;
 script(L,"assert(event.error=='out_of_memory');assert(old.text=='Previous useful page');event=nil;collectgarbage('collect')");
 ink::browser::FileRead file{nullptr,BrowserBody,[](void*,char*,size_t)->size_t{assert(false && "OOM must precede read");return 0;}};
 h.limit=h.used+1024;assert(ink::browser::protectedRead(L,file)==LUA_ERRMEM);lua_pop(L,1);h.limit=4*1024*1024;
 response=128;script(L,"events={};function on_http(e) events[#events+1]=e end");
 for(int i=0;i<5;++i){script(L,"assert(smudge.http_request{url='https://example.org/held'})");assert(dispatch(L,s)==LUA_OK);}
 script(L,"assert(#events==5 and events[5].error=='resource_limit');assert(not smudge.http_store(events[1].handle,'../escape'));assert(smudge.http_store(events[1].handle,'copy-object'));assert(not smudge.http_store(events[1].handle,'copy-object'));for i=1,4 do assert(smudge.http_release(events[i].handle))end");
 assert(std::filesystem::file_size(".pio/browser-binding-data/copy-object")==128);std::filesystem::remove(".pio/browser-binding-data/copy-object");
 Session draining({nullptr,run,defer});draining.allowed=true;draining.domains=domains;strcpy(draining.resourceRoot,s.resourceRoot);
 lua_newtable(L);registerApi(L,draining);lua_setglobal(L,"draining");script(L,"assert(draining.http_request{url='https://example.org/cancel'})");
 assert(Session::busy());draining.close();assert(Session::busy());later(laterArgument);assert(!Session::busy());
 s.allowed=false;script(L,"local id,e=smudge.http_request{url='https://example.org/'};assert(not id and e=='permission_denied')");
 s.close();lua_close(L);assert(h.used==0);assert(!Session::busy());puts("PASS production API5 bindings/manifest/root listing/OOM recovery/opt-out; int32 float32");
}
