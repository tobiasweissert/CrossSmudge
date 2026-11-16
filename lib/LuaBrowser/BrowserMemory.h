#pragma once
#include <cstddef>
namespace ink::browser {
constexpr size_t LuaLimit = 4 * 1024 * 1024;
constexpr size_t PsramReserve = 512 * 1024;
constexpr size_t InternalEmergencyReserve = 16 * 1024;
inline bool permitsResponse(size_t bytes,size_t freePsram){
 return freePsram>=PsramReserve && bytes<=freePsram-PsramReserve;
}
// Used by the real allocator. PSRAM allocation has no internal fallback; the
// small internal floor protects interrupts, while TLS has separate admission.
inline bool permitsGrowth(size_t used,size_t growth,size_t freePsram,size_t freeInternal){
 return used<=LuaLimit && growth<=LuaLimit-used && freePsram>=PsramReserve &&
        growth<=freePsram-PsramReserve && freeInternal>=InternalEmergencyReserve;
}
}
