#include "HalMicrophone.h"
#if defined(FREEINK_DEVICE_STICKY)
#include <Arduino.h>
#include <Logging.h>
bool HalMicrophone::begin() {
  end();
  pinMode(38, OUTPUT); digitalWrite(38, HIGH); delay(20);
  i2s.setPinsPdmRx(19, 20);
  // RX DMA publishes 240 frames at 16 kHz (15 ms); a shorter timeout
  // loses the first block and partial reads at subsequent DMA boundaries.
  i2s.setTimeout(25);
  if (!i2s.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    i2s.end(); digitalWrite(38, LOW);
    LOG_ERR("MIC", "PDM initialization failed"); return false;
  }
  live = true; return true;
}
size_t HalMicrophone::read(uint8_t* out, size_t capacity) {
  return live && out && capacity <= 256 ? i2s.readBytes(reinterpret_cast<char*>(out), capacity) : 0;
}
void HalMicrophone::end() {
  if (live) i2s.end();
  live = false; pinMode(38, OUTPUT); digitalWrite(38, LOW);
}
#endif
