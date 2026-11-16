#pragma once
#include "BrowserLaunch.h"
struct lua_State;
namespace airdrop::browser {
struct LuaLaunchBinding {
  LaunchHandle launch;
  uint32_t attempt = 0;
};
// Adds safe native closures to an existing smudge table. Binding must outlive VM.
void registerLaunchApi(lua_State* state, LuaLaunchBinding& binding);
bool supportsLaunchApi(lua_State* state);
}  // namespace airdrop::browser
