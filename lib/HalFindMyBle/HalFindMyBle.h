#pragma once
#include <cstddef>
#include <cstdint>

namespace hal_findmy_ble {
enum class State : uint8_t { Unavailable, Off, WaitingSync, Ready, Advertising, Fault };
struct Status {
  State state;
  bool ready;
  bool advertising;
  bool uncertain; // Stop/start failure: controller ownership not confirmed idle.
  int error;
};
constexpr uint32_t MaxFrameTtlMs = 60000;
// Single owner, serialized caller on application task (never NimBLE host task).
// No other BLE client may own NimBLE callbacks/legacy advertising. Native
// callbacks only latch atomics; call poll() regularly, then inspect status().
// ready means synchronized host, not proof of pairing/account/native app success.
// begin only initializes the stack; it never advertises. No automatic retry.
bool begin();
// Display/big-endian random-static address; native API reversal is internal.
// Finite monotonic TTL, trustedTime must reflect caller's key/time admission.
bool setFrame(const uint8_t* address, size_t addressSize,
              const uint8_t* payload, size_t payloadSize,
              uint32_t ttlMs, bool trustedTime);
// Erases pending frame even on native failure. Faults require reboot.
bool stop();
void poll();
Status status();
}
