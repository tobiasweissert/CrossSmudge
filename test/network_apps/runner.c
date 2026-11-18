/* Executes production Lua sources using firmware int32/float32 configuration.
 * Host UI/files/network doubles are installed by the tests, never shipped. */
#include <stdio.h>
#include <stdlib.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
_Static_assert(sizeof(lua_Integer) == 4 && sizeof(lua_Number) == 4, "firmware Lua ABI required");
typedef struct {
  size_t used, peak, limit;
} Heap;
static void* allocate(void* ctx, void* p, size_t old, size_t n) {
  Heap* h = ctx;
  if (!p) old = 0;
  if (!n) {
    free(p);
    h->used -= old;
    return NULL;
  }
  if (n > h->limit - (h->used - old)) return NULL;
  void* q = realloc(p, n);
  if (q) {
    h->used = h->used - old + n;
    if (h->used > h->peak) h->peak = h->used;
  }
  return q;
}
int main(int argc, char** argv) {
  Heap h = {0, 0, getenv("LUA_APP_LIMIT") ? strtoul(getenv("LUA_APP_LIMIT"), 0, 10) : 2 * 1024 * 1024};
  lua_State* L = lua_newstate(allocate, &h);
  if (!L || argc != 2) return 2;
  luaL_openlibs(L);
  int rc = luaL_dofile(L, argv[1]);
  if (rc) fprintf(stderr, "%s\n", lua_tostring(L, -1));
  lua_close(L);
  printf("Lua peak %zu / %zu bytes; retained %zu\n", h.peak, h.limit, h.used);
  return rc || h.used;
}
