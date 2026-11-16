#include "LuaRuntime.h"
#include "LuaHttp.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "lauxlib.h"
}
struct Heap {size_t used=0,peak=0,limit=4*1024*1024;
 static void* alloc(void* p,void* block,size_t old,size_t n){auto& h=*(Heap*)p;if(!block)old=0;if(!n){free(block);h.used-=old;return nullptr;}if(n>h.limit-(h.used-old))return nullptr;void* q=realloc(block,n);if(q){h.used=h.used-old+n;if(h.used>h.peak)h.peak=h.used;}return q;}
};
static int testSize(lua_State* L){const char* path=luaL_checkstring(L,1);FILE* file=fopen(path,"rb");if(!file){lua_pushnil(L);return 1;}fseek(file,0,SEEK_END);long size=ftell(file);fclose(file);lua_pushinteger(L,size);return 1;}
static int testRead(lua_State* L){const char* path=luaL_checkstring(L,1);int offset=luaL_checkinteger(L,2),count=luaL_checkinteger(L,3);if(offset<0 || count<0 || count>8192)return luaL_error(L,"test read bound");luaL_Buffer buffer;char* bytes=luaL_buffinitsize(L,&buffer,count);FILE* file=fopen(path,"rb");size_t n=0;if(file){if(!fseek(file,offset,SEEK_SET))n=fread(bytes,1,count,file);fclose(file);}luaL_pushresultsize(&buffer,n);return 1;}
#ifdef BROWSER_FONT_HOST
void registerFontHost(lua_State*);
#endif
int main(int argc,char** argv){assert(argc==2);Heap heap;if(getenv("BROWSER_LUA_LIMIT"))heap.limit=atoi(getenv("BROWSER_LUA_LIMIT"));lua_State* L=lua_newstate(Heap::alloc,&heap);assert(L);assert(ink::runtime::openEssential(L)==LUA_OK);assert(ink::runtime::openNetwork(L,true)==LUA_OK);
#ifdef BROWSER_FONT_HOST
registerFontHost(L);
#endif
lua_register(L,"test_read",testRead);lua_register(L,"test_size",testSize);
int status=luaL_dofile(L,argv[1]);if(status)fprintf(stderr,"%s\n",lua_tostring(L,-1));lua_close(L);assert(heap.used==0);printf("Lua int=%zu float=%zu peak=%zu limit=%zu\n",sizeof(lua_Integer),sizeof(lua_Number),heap.peak,heap.limit);return status?1:0;}
