/* Executes production Lua sources using firmware int32/float32 configuration.
 * Host UI/files/network doubles are installed by the tests, never shipped. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
typedef struct { size_t used, peak, limit; } Heap;
static void *allocate(void *ctx, void *p, size_t old, size_t n) {
  Heap *h = ctx;
  if (!p) old = 0;
  if (!n) { free(p); h->used -= old; return NULL; }
  if (n > h->limit - (h->used-old)) return NULL;
  void *q = realloc(p,n);
  if(q) { h->used=h->used-old+n; if(h->used>h->peak)h->peak=h->used; }
  return q;
}
/* The device's journal lives on SD, outside the Lua heap. */
typedef struct File { char* path; char* data; size_t length; struct File* next; } File;
static File* files;
static File* file(const char* path) {for(File* f=files;f;f=f->next)if(!strcmp(f->path,path))return f;return NULL;}
static int readFile(lua_State* L) {
 File* f=file(luaL_checkstring(L,1)); if(!f){lua_pushnil(L);return 1;}
 size_t n=(size_t)luaL_optinteger(L,2,8192),off=(size_t)luaL_optinteger(L,3,0);
 if(off>f->length)off=f->length;if(n>f->length-off)n=f->length-off;
 lua_pushlstring(L,f->data+off,n);return 1;
}
static int writeFile(lua_State* L) {
 const char* path=luaL_checkstring(L,1);size_t n;const char* data=luaL_checklstring(L,2,&n);
 lua_getglobal(L,"FAIL");int fail=lua_toboolean(L,-1);lua_pop(L,1);if(fail && n>10)n=10;
 File* f=file(path);if(!f){f=calloc(1,sizeof(*f));if(!f)return luaL_error(L,"host OOM");f->path=strdup(path);f->next=files;files=f;}
 int append=lua_toboolean(L,3);size_t offset=append?f->length:0;
 char* next=realloc(f->data,offset+n?offset+n:1);if(!next)return luaL_error(L,"host OOM");
 f->data=next;memcpy(f->data+offset,data,n);f->length=offset+n;
 lua_pushboolean(L,!fail);return 1;
}
static int testBody(lua_State* L) {
  static char body[8192];  /* Host transport input; not on a device task stack. */
  for(size_t i=0;i<sizeof(body);++i)body[i]="word "[i%5];
  if(luaL_optinteger(L,1,0)==1) {
    const char* prefix="Title: Full page\nURL Source: https://example.com/\n\nMarkdown Content:\n";
    char link[80];
    size_t offset=strlen(prefix);memcpy(body,prefix,offset);
    for(int i=0;i<24;++i){snprintf(link,sizeof(link),"[Link %d](https://example.com/path/%d) ",i,i);memcpy(body+offset,link,strlen(link));offset+=strlen(link);}
  }
  lua_pushlstring(L,body,sizeof(body));return 1;
}
int main(int argc, char **argv) {
  Heap h = {0,0, getenv("LUA_APP_LIMIT") ? strtoul(getenv("LUA_APP_LIMIT"),0,10) : 2*1024*1024};
  size_t requested=h.limit; h.limit=2*1024*1024;
  lua_State *L=lua_newstate(allocate,&h);
  if(!L || argc!=2)return 2;
  const luaL_Reg libs[]={{LUA_GNAME,luaopen_base},{LUA_TABLIBNAME,luaopen_table},
    {LUA_STRLIBNAME,luaopen_string},{LUA_MATHLIBNAME,luaopen_math},{LUA_OSLIBNAME,luaopen_os},
    {NULL,NULL}};
  for(const luaL_Reg *lib=libs;lib->func;++lib){luaL_requiref(L,lib->name,lib->func,1);lua_pop(L,1);}
  const char *app=getenv("APP");
  if(app && (!strcmp(app,"weather") || !strcmp(app,"browser"))){luaL_requiref(L,LUA_UTF8LIBNAME,luaopen_utf8,1);lua_pop(L,1);}
  lua_gc(L,LUA_GCCOLLECT,0);
  size_t essential=h.used;
  luaL_requiref(L,LUA_IOLIBNAME,luaopen_io,1);lua_pop(L,1);
  lua_gc(L,LUA_GCINC,105,250,0);
  int rc=luaL_dofile(L,argv[1]);
  lua_pushcfunction(L,testBody);lua_setglobal(L,"TEST_BODY");
  lua_getglobal(L,"smudge");lua_pushcfunction(L,readFile);lua_setfield(L,-2,"read_file");
  lua_pushcfunction(L,writeFile);lua_setfield(L,-2,"write_file");lua_pop(L,1);
  lua_gc(L,LUA_GCCOLLECT,0);
  size_t harness=h.used;
  /* Native UI/files live outside Lua in firmware. Exclude the Lua host double
     and its io library, and reserve the measured production library baseline. */
  h.limit=harness+requested-essential; h.peak=h.used;
  if(!rc)rc=luaL_dostring(L,"RUN_CASE()");
  size_t appPeak=h.peak-harness+essential;
  if(rc)fprintf(stderr,"%s\n",lua_tostring(L,-1));
  lua_close(L);
  printf("App + library peak %zu / %zu bytes (Lua host scaffolding excluded); retained %zu\n",appPeak,requested,h.used);
  while(files){File* f=files;files=f->next;free(f->data);free(f->path);free(f);}
  return rc || h.used;
}
