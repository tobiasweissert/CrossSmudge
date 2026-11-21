#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "LuaRuntime.h"
#include "NotesLaunch.h"
#include "ReviewedResult.h"
static bool denyLarge = false;
static void* alloc(void*, void* ptr, size_t old, size_t n) {
  if (!n) {
    free(ptr);
    return nullptr;
  }
  if (denyLarge && n >= 3000 && (!ptr || n > old)) return nullptr;
  return realloc(ptr, n);
}
static int oom(lua_State* vm) {
  std::string payload(3072, 'z');
  denyLarge = true;
  bool ok = dictation::dispatchReviewed(vm, payload);
  denyLarge = false;
  assert(!ok && dictation::dispatchReviewed(vm, {}));
  return 0;
}
int main(int argc, char** argv) {
  assert(argc == 2 && sizeof(lua_Integer) == 4 && sizeof(lua_Number) == 4);
  auto L = lua_newstate(alloc, nullptr);
  assert(L);
  assert(ink::runtime::openEssential(L) == LUA_OK && ink::runtime::openNetwork(L, false) == LUA_OK);
  lua_register(L, "installNotes", dictation::installNotes);
  lua_register(L, "rejectOOM", oom);
  assert(!dictation::firmwareRoute("/apps/cp-notes", "/apps/cp-notes/notes_015.lua"));
  assert(dictation::firmwareRoute("/.crosssmudge/applications/cp-notes",
                                  "/.crosssmudge/applications/cp-notes/notes_015.lua"));
  int rc = luaL_dofile(L, argv[1]);
  if (rc) fprintf(stderr, "%s\n", lua_tostring(L, -1));
  lua_close(L);
  return rc;
}
