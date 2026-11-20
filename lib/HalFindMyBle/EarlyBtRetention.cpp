#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL && defined(ARDUINO_ARCH_ESP32)
#include "sdkconfig.h"
#endif
// Retain controller memory through Arduino's early initialization. This does
// not initialize the stack or enable advertising. The anchor below ensures
// static archive extraction when the application references begin().
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL && defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
extern "C" bool btInUse() { return true; }
// Referenced by begin so archive linkers also pull this early override TU.
extern "C" void crosscub_findmy_bt_retention_anchor() {}
#endif
