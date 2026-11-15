// Native fixture I/O, guarded by the production resolver and Lua restrictions.
#include "LuaHttp.h"
#include <cassert>
#include <cstdlib>
#include <string>
extern "C" bool integration_allowed(const char* path) {
  const char* id = std::getenv("INTEGRATION_APP");
  std::string resolved;
  return ink::http::resolveAppPath(path, std::string("/.crosssmudge/applications/") + id,
                                 std::string("/.smudge/saved_data/") + id, resolved);
}
extern "C" void integration_sandbox(lua_State* L) {
  assert(!integration_allowed("/books/private.epub"));
  assert(!integration_allowed("/.smudge/settings.json"));
  assert(!integration_allowed("../other/save"));
  assert(integration_allowed("state-a.dat"));
  ink::http::restrictStandardLibraries(L);
}
