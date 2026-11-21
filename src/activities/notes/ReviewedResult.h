#pragma once
#include <string>
extern "C" {
#include "lauxlib.h"
}
namespace dictation {
inline bool dispatchReviewed(lua_State* L, const std::string& text) {
  if (text.size() > 3072 || !lua_checkstack(L, 2)) return false;
  lua_pushcfunction(L, [](lua_State* vm) -> int {
    auto value = static_cast<const std::string*>(lua_touserdata(vm, 1));
    lua_getglobal(vm, "on_dictation_reviewed");
    lua_pushlstring(vm, value->data(), value->size());
    lua_call(vm, 1, 0);
    return 0;
  });
  lua_pushlightuserdata(L, const_cast<std::string*>(&text));
  if (lua_pcall(L, 1, 0, 0) == LUA_OK) return true;
  lua_pop(L, 1);
  return false;
}
}  // namespace dictation
