#pragma once
#include <cstddef>
#include <cstring>
inline bool i2sFail = false, i2sOn = false;
inline int i2sEnds = 0, i2sClock = -1, i2sData = -1;
inline constexpr int I2S_MODE_PDM_RX = 1, I2S_DATA_BIT_WIDTH_16BIT = 16, I2S_SLOT_MODE_MONO = 1;
inline unsigned i2sTimeout=0;
class I2SClass {
 public:
  void setPinsPdmRx(int clock, int data) {
    i2sClock = clock;
    i2sData = data;
  }
  void setTimeout(unsigned timeout) {i2sTimeout=timeout;}
  bool begin(int mode, int rate, int width, int channels) {
    i2sOn = mode == 1 && rate == 16000 && width == 16 && channels == 1;
    return !i2sFail;
  }
  size_t readBytes(char* p, size_t n) {
    if(i2sTimeout<15)return 0; // 240 frames at 16 kHz: first DMA completes at 15 ms.
    memset(p, 0, n);
    return n;
  }
  void end() {
    i2sOn = false;
    ++i2sEnds;
  }
};
