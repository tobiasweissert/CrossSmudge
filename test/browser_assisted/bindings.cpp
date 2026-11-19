#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "LuaHttp.h"
#include "LuaRuntime.h"
using namespace ink::http;
static bool configured = true;
static int imports = 0;
static bool readToken(void*, char (&out)[65]) {
  assert(false && "keyless path read credentials");
  return false;
}
static const char* import(void*) {
  ++imports;
  configured = true;
  return nullptr;
}
static bool random(void*, unsigned char* p, size_t n) {
  for (size_t i = 0; i < n; ++i) p[i] = i;
  return true;
}
static void (*worker)(void*);
static void* arg;
static bool spawn(void (*f)(void*), void* a) {
  worker = f;
  arg = a;
  return true;
}
static void run(void*, Job& j) {
  assert(j.request.backend && j.request.post && j.request.browser);
  assert(!strcmp(j.request.url.host, BackendHost));
  assert(!j.request.authorization[0]);
  assert(j.resource.append("{}", 2));
  j.result.size = 2;
  j.result.status = 200;
  strcpy(j.result.url, BackendOrigin);
  strcpy(j.result.contentType, "application/json");
}
static void script(lua_State* L, const char* s) {
  if (luaL_dostring(L, s)) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    abort();
  }
}
int main() {
  lua_State* L = luaL_newstate();
  assert(L);
  assert(ink::runtime::openEssential(L) == LUA_OK);
  assert(ink::runtime::openNetwork(L, true) == LUA_OK);
  Session s({nullptr, run, spawn});
  s.domains.publicHttps = true;
  s.allowed = true;
  s.backendEnabled = true;
  s.cooperative = true;
  s.backendAccess = {nullptr, readToken, import, random};
  char dir[256];
  snprintf(dir, sizeof(dir), "%s/b7-binding-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  strcpy(s.resourceRoot, dir);
  lua_newtable(L);
  registerApi(L, s);
  lua_setglobal(L, "smudge");
  restrictStandardLibraries(L);
  script(L,
         "assert(smudge.browser_backend_configured());assert(smudge.browser_request_id()=='"
         "000102030405060708090a0b0c0d0e0f');function on_http(e) event=e;local "
         "id,err=smudge.browser_backend_request{path='/v1/poll',body='{}'};assert(not id and err=='busy')end");
  script(L,
         "for _,q in "
         "ipairs({{path='/v1/open?leak',body='{}'},{path='https://evil.org/',body='{}'},{path='/v1/"
         "open',body='{}',headers={}},{path='/v1/open',body='{}',token='synthetic'},{path='/v1/"
         "open',body='{}',timeout_ms=999},{path='/v1/open',body=string.rep('a',2049)}})do assert(not "
         "smudge.browser_backend_request(q))end;assert(not "
         "smudge.http_request{url='https://example.org/',method='POST'});assert(not "
         "smudge.http_request{url='https://example.org/',headers={Authorization='Bearer synthetic'}})");
  script(L,
         "id=assert(smudge.browser_backend_request{path='/v1/open',body='{}'});local "
         "ok,err=smudge.browser_backend_configure();assert(not ok and err=='busy')");
  assert(imports == 0);
  worker(arg);
  assert(dispatch(L, s) == LUA_OK);
  script(L,
         "assert(event.handle and not event.body and not "
         "event.token);assert(smudge.http_read(event.handle,0,2)=='{}');assert(smudge.http_release(event.handle));"
         "assert(smudge.browser_backend_configure())");
  assert(imports == 0);
  configured = false;
  script(L, "assert(smudge.browser_backend_configured())");
  configured = true;
  script(L, "id=assert(smudge.browser_backend_request{path='/v1/open',body='{}'});assert(smudge.http_cancel(id))");
  s.close();
  assert(Session::busy());
  worker(arg);
  assert(!Session::busy());
  s.allowed = false;
  script(L, "assert(not smudge.browser_backend_request{path='/v1/open',body='{}'})");
  Session old({nullptr, run, spawn});
  lua_newtable(L);
  registerApi(L, old);
  lua_setglobal(L, "old");
  script(L, "assert(not old.browser_backend_request and not old.browser_request_id)");
  lua_close(L);
  std::filesystem::remove_all(dir);
  puts("PASS actual API7 registration/ordinary negative controls/config/busy/callback/drain/resource ownership");
}
