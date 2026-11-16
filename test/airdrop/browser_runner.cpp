#include "LuaRuntime.h"
#include "BrowserWork.h"
#include "LuaLaunch.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "lauxlib.h"
}
using namespace airdrop::browser;
static LaunchHandle coordinator;
static LuaLaunchBinding bridge;
int main(int argc,char** argv){
  assert(argc==2);
  lua_State* state=luaL_newstate();assert(state);
  assert(ink::runtime::openEssential(state)==LUA_OK);
  assert(ink::runtime::openNetwork(state,true)==LUA_OK);
  assert(ink::browser::openBrowserWork(state)==LUA_OK);
  lua_newtable(state);ink::browser::registerWork(state);lua_setglobal(state,"browser_native");
  lua_newtable(state);registerLaunchApi(state,bridge);lua_setglobal(state,"native_launch");
  lua_register(state,"test_prepare_launch",(+[](lua_State* L){
    auto owned=std::unique_ptr<LaunchRequest>(new LaunchRequest);
    assert(owned->prepare(luaL_checkstring(L,1)));coordinator=LaunchHandle(std::move(owned));
    bridge.launch=coordinator;bridge.attempt=coordinator->beginAttempt();assert(bridge.attempt);return 0;
  }));
  lua_register(state,"test_clear_launch",(+[](lua_State*){bridge={};coordinator={};return 0;}));
  lua_register(state,"test_launch_state",(+[](lua_State* L){lua_pushinteger(L,coordinator?int(coordinator->state()):int(LaunchState::Empty));return 1;}));
  lua_register(state,"test_supports_launch",(+[](lua_State* L){lua_pushboolean(L,supportsLaunchApi(L));return 1;}));
  lua_register(state,"test_read",(+[](lua_State* L){
    const char* path=luaL_checkstring(L,1);int offset=luaL_checkinteger(L,2),count=luaL_checkinteger(L,3);
    if(offset<0||count<0||count>24657)return luaL_error(L,"test read bound");
    FILE* file=fopen(path,"rb");if(!file){lua_pushnil(L);return 1;}
    luaL_Buffer buffer;char* bytes=luaL_buffinitsize(L,&buffer,count);size_t n=0;
    if(!fseek(file,offset,SEEK_SET))n=fread(bytes,1,count,file);fclose(file);luaL_pushresultsize(&buffer,n);return 1;
  }));
  lua_register(state,"test_size",(+[](lua_State* L){FILE* f=fopen(luaL_checkstring(L,1),"rb");if(!f){lua_pushnil(L);return 1;}fseek(f,0,SEEK_END);long n=ftell(f);fclose(f);lua_pushinteger(L,n);return 1;}));
  lua_register(state,"test_fs_list",(+[](lua_State* L){lua_newtable(L);return 1;}));
  lua_register(state,"test_fs_remove",(+[](lua_State* L){lua_pushboolean(L,true);return 1;}));
  int result=luaL_dofile(state,argv[1]);if(result)std::fprintf(stderr,"%s\n",lua_tostring(state,-1));
  lua_close(state);bridge={};coordinator={};std::printf("Lua int=%zu float=%zu\n",sizeof(lua_Integer),sizeof(lua_Number));return result?1:0;
}
