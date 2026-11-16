#pragma once
// Browser-only cooperative Lua execution. Each resume returns to the activity
// loop, including from nested Lua pcall; never yield through a native C frame.
#include <chrono>
extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}
namespace ink::browser {
struct Slice {
  std::chrono::steady_clock::time_point deadline;
  unsigned instructions = 0;
};
inline void sliceHook(lua_State* L, lua_Debug*) {
  auto* slice = *static_cast<Slice**>(lua_getextraspace(L));
  slice->instructions += 500;
  if (lua_isyieldable(L) && (slice->instructions >= 50000 || std::chrono::steady_clock::now() >= slice->deadline))
    lua_yield(L, 0);
}
inline int resumeWork(lua_State* L) {
  auto* co = lua_tothread(L, 1);
  if (!co || co == L) return luaL_error(L, "invalid browser coroutine");
  Slice slice{std::chrono::steady_clock::now() + std::chrono::milliseconds(8)};
  *static_cast<Slice**>(lua_getextraspace(co)) = &slice;
  lua_sethook(co, sliceHook, LUA_MASKCOUNT, 500);
  int results = 0;
  int status = lua_resume(co, L, 0, &results);
  lua_sethook(co, nullptr, 0, 0);
  *static_cast<Slice**>(lua_getextraspace(co)) = nullptr;
  lua_pushboolean(L, status == LUA_OK || status == LUA_YIELD);
  if (status != LUA_OK && status != LUA_YIELD) lua_xmove(co, L, 1);
  else { lua_pop(co, results); lua_pushnil(L); }
  lua_pushinteger(L, slice.instructions);
  return 3;
}
inline int atomicWork(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  // Deliberately non-yieldable: only the short, checked index publication and
  // matching Lua view swap belong here. Parse/copy/checksum run before it.
  lua_settop(L, 1);
  lua_call(L, 0, 0);
  return 0;
}
inline int openWork(lua_State* L) {
  luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
  lua_pop(L, 1);
  return 0;
}
inline int openBrowserWork(lua_State* L) {
  lua_pushcfunction(L, openWork);
  return lua_pcall(L, 0, 0, 0);
}
inline void registerWork(lua_State* L) {
  lua_pushcfunction(L, resumeWork); lua_setfield(L, -2, "browser_resume");
  lua_pushcfunction(L, atomicWork); lua_setfield(L, -2, "browser_atomic");
}
} // namespace ink::browser
