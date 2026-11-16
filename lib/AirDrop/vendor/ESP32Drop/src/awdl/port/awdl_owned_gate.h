// CrossSmudge adaptation: allocation-free callback admission/lifetime accounting.
#pragma once
#include <stdint.h>
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <freertos/FreeRTOS.h>
// These callbacks run on the pinned WiFi/frame tasks, not in an ISR. The S3
// PSRAM toolchain deliberately disables C++ hardware atomics. SDK portMUX
// critical sections provide cross-core acquire/release without libatomic locks
// or changing that silicon-workaround compiler setting. Never wait inside them.
namespace awdl_owned {
class Gate {
 public:
  bool enter() {
    portENTER_CRITICAL(&mux_);
    bool accepted=!(state_&Closed)&&(state_&Count)!=Count;
    if(accepted)++state_;
    portEXIT_CRITICAL(&mux_);
    return accepted;
  }
  void leave(){portENTER_CRITICAL(&mux_);--state_;portEXIT_CRITICAL(&mux_);}
  void close(){portENTER_CRITICAL(&mux_);state_|=Closed;portEXIT_CRITICAL(&mux_);}
  bool drained()const{portENTER_CRITICAL(&mux_);bool done=(state_&Count)==0;portEXIT_CRITICAL(&mux_);return done;}
  bool open(){portENTER_CRITICAL(&mux_);bool ok=state_==Closed;if(ok)state_=0;portEXIT_CRITICAL(&mux_);return ok;}
 private:
  static constexpr uint32_t Closed=0x80000000u,Count=~Closed;
  mutable portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
  uint32_t state_=Closed;
};
}
#else
#include <atomic>
namespace awdl_owned {
class Gate {
 public:
  bool enter(){uint32_t old=state_.load(std::memory_order_acquire);for(;;){if((old&Closed)||(old&Count)==Count)return false;if(state_.compare_exchange_weak(old,old+1,std::memory_order_acq_rel,std::memory_order_acquire))return true;}}
  void leave(){state_.fetch_sub(1,std::memory_order_release);}
  void close(){state_.fetch_or(Closed,std::memory_order_acq_rel);}
  bool drained()const{return (state_.load(std::memory_order_acquire)&Count)==0;}
  bool open(){uint32_t expected=Closed;return state_.compare_exchange_strong(expected,0,std::memory_order_acq_rel);}
 private:
  static constexpr uint32_t Closed=0x80000000u,Count=~Closed;
  std::atomic<uint32_t> state_{Closed};
};
}
#endif
namespace awdl_owned {
class Use {
 public:
  explicit Use(Gate& gate):gate_(gate),entered_(gate.enter()){}
  ~Use(){if(entered_)gate_.leave();}
  explicit operator bool()const{return entered_;}
  Use(const Use&)=delete;Use& operator=(const Use&)=delete;
 private:Gate& gate_;bool entered_;
};
}
