#pragma once
#include <string_view>

#include "NotesFirmware.h"
extern "C" {
#include "lauxlib.h"
}
namespace dictation {
inline bool firmwareRoute(std::string_view dir, std::string_view script) {
  return dir == "/.crosssmudge/applications/cp-notes" && script == "/.crosssmudge/applications/cp-notes/notes_015.lua";
}
inline int loadModule(lua_State* vm) {
  const char* src = dictation::module(luaL_checkstring(vm, 1));
  if (!src) return luaL_error(vm, "Unknown firmware Notes module");
  if (luaL_loadbufferx(vm, src, strlen(src), "Notes", "t")) return lua_error(vm);
  lua_call(vm, 0, 1);
  return 1;
}
inline int installNotes(lua_State* vm) {
  lua_pushnil(vm);
  lua_setglobal(vm, "load");
  lua_pushcfunction(vm, loadModule);
  lua_setglobal(vm, "dofile");
  lua_getglobal(vm, "smudge");
  lua_pushcfunction(vm, loadModule);
  lua_setfield(vm, -2, "dofile");
  lua_pushvalue(vm, 1);
  lua_setfield(vm, -2, "request_dictation");
  lua_pop(vm, 1);
  lua_pushcfunction(vm, loadModule);
  lua_pushliteral(vm, "sticky_enhanced.lua");
  lua_call(vm, 1, 1);
  return 1;
}
}  // namespace dictation
