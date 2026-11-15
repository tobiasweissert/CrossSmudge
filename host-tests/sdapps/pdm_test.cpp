#include <Arduino.h>

#include <cassert>
#include <cstdio>

#include "PdmMicrophone.h"
int main() {
  stickychat::PdmMicrophone mic;
  assert(micPower == 0 && !i2sOn);
  assert(mic.begin());
  assert(micPower == 1 && i2sOn && i2sClock == 19 && i2sData == 20);
  uint8_t bytes[192];
  assert(mic.read(bytes,sizeof(bytes))==sizeof(bytes));
  assert(i2sTimeout>=15 && i2sTimeout<=50);
  mic.end();
  assert(!micPower && !i2sOn);
  i2sFail = true;
  assert(!mic.begin());
  assert(!micPower && !i2sOn);
  std::puts("production PDM adapter pins/power/failure cleanup (fake I2S): PASS");
}
