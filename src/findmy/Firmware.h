#pragma once
namespace crosscub::findmy::firmware {
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL
bool ownerEnableFresh();
bool ownerEnableExisting();
void poll();
bool standby();
bool beforeAirDrop();
bool beforeSleep();
const char* statusText();
#else
inline void poll() {}
inline bool standby(){return false;}
inline bool beforeAirDrop(){return true;}
inline bool beforeSleep(){return true;}
#endif
}
