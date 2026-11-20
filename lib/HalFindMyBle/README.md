# Experimental native NimBLE beacon transport

This is a native ESP32-S3 advertising adapter, **not** official Find My pairing,
account registration, certification, or evidence of success in Apple's Items UI.
No GATT services, sound service, scan, connection, account, or network operations
are implemented. No real radio has been activated for qualification.

## Build / ownership

Compile both `HalFindMyBle.cpp` and `EarlyBtRetention.cpp`. Native code requires
`CROSSCUB_FINDMY_EXPERIMENTAL=1`, `ARDUINO_ARCH_ESP32`, and
`CONFIG_IDF_TARGET_ESP32S3` from the existing SDK's `sdkconfig.h`. All other builds
are inert and require no NimBLE/Arduino headers. The experiment also requires an
SDK built with the native NimBLE host, broadcaster/legacy advertising, controller,
FreeRTOS, and esp_timer components. Do not use ArduinoBLE / NimBLE-Arduino.

`EarlyBtRetention.cpp` supplies strong `extern "C" bool btInUse()` **only** for
the experiment on S3, retaining Arduino's controller memory before setup. Its
link anchor in `begin()` forces static-archive extraction despite Arduino's weak
symbol. This retains memory; it does not start the stack or advertising. Native
firmware linking still needs verification against its complete link graph.

One application task must serialize all public API calls, never from a NimBLE
callback/host task. The adapter exclusively owns NimBLE sync/reset callbacks and
legacy advertising; coexistence with another BLE owner is not supported. Let
Arduino initialize normal platform/NVS infrastructure before `begin()`.

## API / integration

```cpp
#include <HalFindMyBle.h>
// Include your public-frame producer separately; the transport has no crypto.
namespace ble = hal_findmy_ble;

// Only after explicit feature opt-in / tracking admission:
bool accepted = ble::begin(); // initialization only, asynchronous host sync

// On a newly admitted public frame, not on every loop iteration:
// ttlMs must be <= remaining key validity and <= ble::MaxFrameTtlMs (60000).
// trustedTime is supplied by caller policy, not inferred by the transport.
accepted = ble::setFrame(frame.address.data(), frame.address.size(),
                         frame.payload.data(), frame.payload.size(),
                         ttlMs, trustedTime);
// accepted means queued, NOT necessarily on-air. Never retry automatically.

// Each application loop, outside the NimBLE host task:
ble::poll();
auto s = ble::status();
// s.state: Unavailable / Off / WaitingSync / Ready / Advertising / Fault
// s.ready: host synchronized, may also be advertising
// s.advertising: confirmed successful native start, not uncertain ownership
// s.uncertain: cancellation/start outcome cannot yet be confirmed idle
// s.error: native error; -1 sync timeout, -2 inconsistent stop,
//          -3 reset without reason, -4 callback tag exhaustion

// Opt-out, untrusted clock, invalid key admission, owner replacement:
bool confirmedIdle = ble::stop(); // always erases pending frame
// If false or s.uncertain, do not claim RF stopped. Remain disabled / faulted.
```

`setFrame(const uint8_t*, size_t, const uint8_t*, size_t, uint32_t, bool)`
requires exactly 6 display-order (big-endian) random-static MAC bytes and 31 raw
advertisement bytes. It validates the manufacturer-data envelope
`1e ff 4c 00 12 19`, restored high-key bits <=3, random-static address type,
and excludes all-zero/all-one random address bits. Malformed input or untrusted
clock clears/stops the previous frame rather than retaining it. Input is copied.
No private keys are accepted or retained.

The cached SDK's `host/ble_hs_id.h` explicitly documents
`ble_hs_id_set_rnd()` as **host little-endian**. This adapter reverses the six
input bytes only at that boundary. Advertising data is passed byte-for-byte;
scan response is explicitly empty. Parameters are non-connectable,
non-discoverable, random own address; no local name is published.

Every frame has a monotonic deadline. Time spent waiting for sync reduces its
remaining lifetime; expired queued frames cannot start. The native advertising
procedure also receives the remaining **finite** duration, never
`BLE_HS_FOREVER`, so missed application polls do not intentionally renew old keys.
Caller must calculate TTL from trusted absolute key expiration and must not
re-submit an expired key as a new frame. Completion/expiry never restarts a frame.

A replacement stops the old procedure in `setFrame()` and starts the new one only
on a later `poll()` call. The cached `ble_gap.h` says successful stop is complete
and does not require an ADV_COMPLETE callback, but warns against stop/start in the
same NimBLE host execution context. Use the application task contract above.
Late completion tags cannot clear a newer procedure or overwrite its completion.

Initialization failure, 5-second sync timeout, host reset, or any native setup,
start, or stop failure is terminal for this boot: pending frame is erased and
there is no automatic resync/restart/retry. Faults attempt cancellation once;
subsequent polls only reconcile observed idle. Unknown ownership remains explicit
until native `ble_gap_adv_active()` confirms idle, **not** merely because a deadline
elapsed. The finite native duration is a second safety bound, not proof of RF
cessation when the native stack/controller is faulty. No stack deinit/reinit is
attempted because that could reuse unknown controller ownership.

## Qualification

Run from repository root:

```
python3 test/findmy/transport/run.py
SANITIZE=1 python3 test/findmy/transport/run.py
python3 test/findmy/transport/sdk_syntax.py
```

The first two compile **the actual production adapter** against controlled native
C API headers/functions and synthetic public frame bytes; they do not replace the
adapter with a model. Sanitizer mode enables UBSan. Tests cover default-off,
asynchronous sync/reset, timeout, exact native address/data capture, finite native
duration, queued/active expiry, rejection/clearing, failure transitions, opt-out,
and late completion ownership. An archive-link regression verifies strong early
retention in the presence of an Arduino-like weak symbol.

The SDK check uses the already-installed Xtensa compiler and real ESP32-S3
headers/config read-only, without forced preinclude of `sdkconfig.h`, and verifies
that the native branch and early retention were not preprocessed away. This is
**target syntax checking only**, not actual SDK linking, firmware build, flashing,
controller runtime, over-air reception, Apple account acceptance, or Items UI
qualification. The synthetic archive link is not an ESP-IDF link.

All executables and RED/GREEN/SDK logs go outside the worktree to
`/root/.hermes/task-state/crosscub-findmy-implementation/transport/`.
