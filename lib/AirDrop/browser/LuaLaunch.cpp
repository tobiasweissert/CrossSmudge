#include "LuaLaunch.h"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
namespace airdrop::browser {
namespace {
LuaLaunchBinding* binding(lua_State* state) { return static_cast<LuaLaunchBinding*>(lua_touserdata(state,lua_upvalueindex(1))); }
int take(lua_State* state) {
  auto* value = binding(state);
  const char* url = value && value->launch ? value->launch->take(value->attempt) : nullptr;
  if (!url) { lua_pushnil(state); return 1; }
  lua_pushstring(state,url);
  lua_pushinteger(state,static_cast<lua_Integer>(value->attempt));
  return 2;
}
int acknowledge(lua_State* state) {
  auto* value = binding(state);
  lua_Integer token = luaL_checkinteger(state,1);
  bool accepted = value && value->launch && token > 0 && token == static_cast<lua_Integer>(value->attempt) &&
                  value->launch->acknowledge(value->attempt);
  lua_pushboolean(state,accepted);return 1;
}
int fail(lua_State* state) {
  auto* value = binding(state);
  bool retained = value && value->launch && value->launch->failAttempt(value->attempt,LaunchError::ScriptFailed);
  lua_pushboolean(state,retained);return 1;
}
void add(lua_State* state, const char* name, lua_CFunction fn, LuaLaunchBinding& value) {
  lua_pushlightuserdata(state,&value);lua_pushcclosure(state,fn,1);lua_setfield(state,-2,name);
}
}
void registerLaunchApi(lua_State* state, LuaLaunchBinding& value) {
  add(state,"take_launch_url",take,value);
  add(state,"ack_launch_url",acknowledge,value);
  add(state,"fail_launch_url",fail,value);
}
bool supportsLaunchApi(lua_State* state) {
  lua_getglobal(state,"browser_launch_api");
  bool supported = lua_isinteger(state,-1) && lua_tointeger(state,-1) == 1;
  lua_pop(state,1);return supported;
}
}  // namespace airdrop::browser
