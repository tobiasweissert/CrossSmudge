#pragma once
extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}

namespace ink::runtime {
// Keep the offline library set unchanged. Library allocations belong to the
// bounded Lua heap; protected calls turn startup OOM into a recoverable error.
inline int essential(lua_State* L) {
  static const luaL_Reg libraries[] = {{LUA_GNAME, luaopen_base},        {LUA_TABLIBNAME, luaopen_table},
                                       {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},
                                       {LUA_OSLIBNAME, luaopen_os},      {nullptr, nullptr}};
  for (const luaL_Reg* lib = libraries; lib->func; ++lib) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);
  }
  return 0;
}
inline int network(lua_State* L) {
  luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_pop(L, 1);
  return 0;
}
inline int openEssential(lua_State* L) {
  lua_pushcfunction(L, essential);
  return lua_pcall(L, 0, 0, 0);
}
// Valid network manifests get native UTF-8 even when networking is opted out:
// cached JSON and text need exactly the same decoding as fetched responses.
inline int openNetwork(lua_State* L, bool declared) {
  if (!declared) return LUA_OK;
  lua_pushcfunction(L, network);
  return lua_pcall(L, 0, 0, 0);
}
}  // namespace ink::runtime
