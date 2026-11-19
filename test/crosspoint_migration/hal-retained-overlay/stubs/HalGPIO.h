#pragma once
class HalGPIO {
 public:
  enum class WakeupReason { PowerButton, AfterFlash, Other };
  WakeupReason getWakeupReason() const;
};
extern HalGPIO gpio;
