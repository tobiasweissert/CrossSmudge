#include "BrowserMemory.h"
#include <cassert>
using namespace ink::browser;
int main(){
 assert(!permitsResponse(768*1024,800*1024));
 assert(permitsResponse(768*1024,768*1024+PsramReserve));
 assert(permitsGrowth(1024,8192,1024*1024,32*1024)); // TLS may consume internal SRAM; Lua is PSRAM-only.
 assert(!permitsGrowth(1024,8192,1024*1024,15*1024));
 assert(!permitsGrowth(LuaLimit-1024,1025,8*1024*1024,128*1024));
 assert(!permitsGrowth(1024,8192,PsramReserve+8191,128*1024));
 assert(permitsGrowth(LuaLimit-1024,1024,PsramReserve+1024,16*1024));
}
