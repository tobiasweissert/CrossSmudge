#pragma once
#if defined(FREEINK_DEVICE_STICKY)
#include <HalMicrophone.h>
#include "ChatCapture.h"
namespace stickychat {
class PdmMicrophone final : public Microphone {
 public:
  bool begin() override;
  size_t read(uint8_t*, size_t) override;
  void end() override;
 private:
  HalMicrophone microphone;
};
}
#endif
