#pragma once
#include <cstddef>
extern "C" {
#include "lauxlib.h"
}
namespace ink::browser {
struct FileRead {
 void* context;
 size_t bytes;
 size_t (*read)(void*,char*,size_t);
};
inline int readBuffer(lua_State* L){
 auto* file=static_cast<FileRead*>(lua_touserdata(L,1));
 luaL_Buffer buffer;char* bytes=luaL_buffinitsize(L,&buffer,file->bytes);
 size_t count=file->read(file->context,bytes,file->bytes);
 luaL_pushresultsize(&buffer,count);return 1;
}
// The caller owns/explicitly closes the file after this protected call. Lua
// allocation errors cannot longjmp over its C++ file/path ownership anymore.
inline int protectedRead(lua_State* L,FileRead& file){
 lua_pushcfunction(L,readBuffer);lua_pushlightuserdata(L,&file);
 return lua_pcall(L,1,1,0);
}
}
