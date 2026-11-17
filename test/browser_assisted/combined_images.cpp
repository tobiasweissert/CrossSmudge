// Host adapter, real pinned PNGdec -> production asynchronous PXC -> real blit.
#include "BrowserImage.h"
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <chrono>
#include <thread>
#include <cassert>
extern "C" {
#include "lauxlib.h"
}
static ink::browser::ImageSession images;
static int decode(lua_State* L){
 size_t n;const char* b=luaL_checklstring(L,1,&n);int w=luaL_checkinteger(L,2),h=luaL_checkinteger(L,3);
 Storage.put("/combined.png",std::vector<uint8_t>(b,b+n));
 const char* err=images.start("/combined.png","/combined.pxc",w,h);if(err)return luaL_error(L,"codec start %s",err);
 int state=0;auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!(state=images.poll())&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
 assert(state==1);assert(!Storage.openHandles("/combined.png"));
 auto& data=Storage.bytes("/combined.pxc");lua_pushlstring(L,(const char*)data.data(),data.size());return 1;
}
static int blit(lua_State* L){
 size_t n;const char* b=luaL_checklstring(L,1,&n);Storage.put("/combined-blit.pxc",std::vector<uint8_t>(b,b+n));
 GfxRenderer renderer;std::atomic<bool> cancel{false};bool ok=images.draw("/combined-blit.pxc",renderer,0,0,128,64,cancel);assert(ok);
 auto* p=renderer.getWriteTarget();unsigned black=0;for(int y=0;y<64;y++)for(int x=0;x<128;x++)black+=(p[y*100+x/8]&(0x80>>(x%8)))==0;
 assert(black==4096);assert(!Storage.openHandles("/combined-blit.pxc"));lua_pushboolean(L,true);return 1;
}
void registerCombinedImages(lua_State* L){lua_register(L,"combined_decode",decode);lua_register(L,"combined_blit",blit);}
