#include "PdmMicrophone.h"
#if defined(FREEINK_DEVICE_STICKY)
namespace stickychat {
bool PdmMicrophone::begin() { return microphone.begin(); }
size_t PdmMicrophone::read(uint8_t* out, size_t size) { return microphone.read(out, size); }
void PdmMicrophone::end() { microphone.end(); }
}
#endif
