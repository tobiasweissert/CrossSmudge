# Crosscub Find My / OpenHaystack feasibility investigation

Status: initial investigation complete; feature not implemented or physically qualified.

**Follow-up:** the owner requires tracking during apparent sleep and genuine native
Apple Find My pairing, with OTA handled elsewhere. See
[Native pairing and tracking standby](findmy-native-pairing-and-standby.md) for the
current feasibility assessment; it supersedes the powered-on-only recommendation
below for that goal.

## Verdict

**A powered-on Find My-compatible BLE beacon is a credible experimental feature
for the Sticky's ESP32-S3. It is not a drop-in OpenHaystack port or an AirTag
replacement. The current sleep behavior is the main product limitation.**

- Powered on/ordinary idle: conditional on integration, image/RAM fit and tests.
- E-paper sleep screen under today's firmware: no beacon, because this enters
  system deep sleep. A new screen-only tracking standby could keep BLE alive,
  at an unmeasured battery cost.
- Actual system deep sleep: cannot advertise. Timer-wake bursts are a possible
  separate design; packets would be sent while awake, not during deep sleep.
- Supply physically removed: the internal radio cannot advertise. A separately
  powered tracker would be an additional hardware solution.

BLE compatibility does not establish native Find My Items registration, UWB
Precision Finding, official certification, a sound-command channel or guaranteed
Apple-device relaying. Real packet reception, Apple relay, report decryption and
battery performance remain untested.

## Scope and baseline

- Owner repository: `tobiasweissert/Crosscub`.
- Branch: `research/findmy-tracking`.
- Worktree: `/root/github/crosscub-findmy-research`.
- Verified owner-main base: `818b0befac4d9249c7abded67fa980187c4b41af`.
- SDK Gitlink: `b784446302c076b4b5a622627867f0c80905cc50`.
- Deliberately sparse checkout: `src`, `include`, `docs`, root files. Absent paths
  were read with `git show`, without initializing the SDK or changing shared builds.
- No firmware source changes, SDK installation, target compilation, commit/push,
  release, physical flashing, beacon activation, Apple login or real keys.
- Low free space on the repository volume blocked treating a build as a safe
  next action without a fresh capacity check and separate authorization.

## Protocol and reproducible offline probe

Original OpenHaystack inspected commit:
`ea05dadb9e6e71075ef08afb5c2087302fbb7210`.

- [Firmware](https://github.com/seemoo-lab/openhaystack/blob/ea05dadb9e6e71075ef08afb5c2087302fbb7210/Firmware/ESP32/main/openhaystack_main.c)
- [ESP32 documentation](https://github.com/seemoo-lab/openhaystack/blob/ea05dadb9e6e71075ef08afb5c2087302fbb7210/Firmware/ESP32/README.md)

The beacon needs a 28-byte public advertisement key: the big-endian x coordinate
of a P-224 public key. Six bytes contribute to its random-static Bluetooth
address; the remaining key material and displaced address bits fit a 31-byte
legacy manufacturer advertisement. No GATT pairing is implemented by this frame.
The original port targets classic ESP32/ESP-IDF 4.2 and uses Bluedroid APIs.

Run:

```sh
python3 -B docs/research/findmy_packet_probe.py
```

Four test methods pass: synthetic round-trip coverage across all 256 values of
the first key byte, bad key lengths, invalid frames and the legacy payload budget.
The probe uses no radio, network, owner keys or SDK. It checks framing, not valid
EC points, security, endianness at a real controller API or Apple acceptance.
Because the frame fills the 31-byte budget, a wrapper must not append another
Flags AD structure. Address/payload changes must be coordinated and scanner-tested.

## Concrete firmware blockers and integration boundaries

### 1. Retain Bluetooth memory before setup

The inspected Arduino 3.3.7 startup calls `initArduino()` before `setup()`.
If the weak `btInUse()` hook remains false, startup calls
`esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)`. Successfully released
controller memory cannot be recovered during that boot. Calling BLE init only
from `setup()` is therefore insufficient.

References in the existing framework cache:

- `cores/esp32/main.cpp:112-113`.
- `cores/esp32/esp32-hal-misc.c:291-294,345-348`.
- `cores/esp32/esp32-hal-bt.c:20-28`.

A native component must participate in the early retention mechanism, without
assuming persistent settings are readable in a pre-setup hook. Same-boot off/on
must stop/deinitialize without irreversibly releasing memory.

[Arduino startup source](https://github.com/espressif/arduino-esp32/blob/3.3.7/cores/esp32/esp32-hal-misc.c)

### 2. Use the actual host, not a copied Bluedroid example

`platformio.ini` ignores the Arduino `BLE` wrapper; that does not prove the IDF
controller is absent. The inspected cached ESP32-S3 configuration enables BT,
NimBLE, controller and broadcaster support, but disables Bluedroid. Actual cached
`libbt.a` defines `nimble_port_init`, `ble_hs_id_set_rnd`,
`ble_gap_adv_set_data` and `ble_gap_adv_start`. The original OpenHaystack
Bluedroid initialization is not a drop-in for that host.

This is evidence from an existing cache, not an authenticated link map/config
for a new exact-release build. Candidate compilation and runtime remain required.
The inspected cache also disables `CONFIG_PM_ENABLE` and BT controller modem
sleep; low-power automatic light sleep cannot be assumed.

### 3. Existing sleep explicitly stops RF operation

- `src/main.cpp:1059-1108,1716-1724`: sleep and timeout route into deep sleep.
- `lib/hal/HalPowerManager.cpp:45-86`: ordinary power saving reduces CPU frequency;
  it is not system light sleep and has no Bluetooth-aware policy.
- `lib/hal/HalPowerManager.cpp:92-146`: stop Wi-Fi, retain main power latches,
  switch off peripheral rails, arm power-button wake, enter SoC deep sleep.

On Sticky, GPIO45/46 remain HIGH through normal firmware deep sleep. That is not
physical supply removal. The retained e-paper image does not mean BLE is alive.
A new tracking standby must preserve input/wake, storage and reader semantics;
current sleep also needs ordered BLE shutdown if a beacon is added.

[Espressif S3 sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html)

### 4. Coordinate AirDrop explicitly

Native AWDL/AirDrop owns raw/promiscuous Wi-Fi on channel 6 and disables Wi-Fi
power saving. `WiFi.getMode()` alone does not represent that radio ownership.
Initially pause BLE for AirDrop and resume only after shutdown is joined and
ownership is conclusively released. Standard BLE/Wi-Fi coexistence does not
qualify custom AWDL timing.

Boundaries: `lib/hal/HalAirDropRadio.cpp:9-25`,
`lib/AirDrop/port/ReceiverEsp32.cpp:380-394`,
`lib/AirDrop/vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.cpp:2115-2153,2484-2514`,
`src/activities/network/AirDropReceiveActivity.cpp:64-109`.

[Espressif coexistence documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-guides/coexist.html)

### 5. Tight image budget

Observed stable `crosscub-v1.6.5.5` targets the research base. Published application:
6,337,616 bytes; OTA partition: 6,553,600 bytes; remaining: 215,984 bytes.
This is not a Bluetooth candidate measurement. Check image fit and internal heap
under reader/network load without weakening OTA checks or changing partitions.

## Collector, security and ecosystem

A separate owner-controlled collector is needed for these custom keys; native
Find My app pairing must not be promised. Prefer evaluating pinned MIT-licensed
[FindMy.py](https://github.com/malmeloo/FindMy.py/tree/22e4879426f97c9b0a1a33b22dac34ef0cf3d02f)
(commit `22e4879426f97c9b0a1a33b22dac34ef0cf3d02f`) rather than deploying a large
unreviewed all-in-one stack.

- Beacon receives public advertisement keys only. The collector retains private
  decryption keys and authenticated Apple/Anisette state off-device.
- Public advertisement keys and private P-224 scalars can both be 28 bytes.
  Length alone cannot distinguish them; use explicit types/provisioning formats.
- FindMy.py documents an Apple account with a previously attached device and
  password/2FA authentication. Account acceptance, refresh longevity and real
  Crosscub reports were not tested. This is reverse-engineered service access,
  not an Apple-supported public report API or Sign in with Apple OAuth.
- Entire account exports contain sensitive credentials/tokens/provisioning state;
  encrypt them and never log them. Show original observation age, not fetch time.
- Original OpenHaystack uses a persistent identifier. Static keys allow local
  linkability; cycling a finite key list still links on reuse and is not Apple's
  derived rolling-key design. Public-key replay also means a location report is
  not authenticated proof of this physical reader's presence.
- S3 examples exist in ESPHome and a separate macless S3 port, but example config
  and author-reported hardware results do not qualify Crosscub power/RF/relay.
- Do not import inspected macless-haystack unchanged: key generation uses
  non-cryptographic `random.getrandbits`; authentication disables TLS verification
  in some paths and logs sensitive headers. The ESPHome component also owns NVS
  initialization/erase and Bluetooth lifecycle, unsuitable for blind integration.
- Do not recommend historical Mail plugins or disabling SIP/Gatekeeper.

Pinned alternative references:

- [ESPHome S3 example](https://github.com/barrenechea/esphome-config-files/blob/a95477c2134a441384fc86fe006828d4b01ac036/openhaystack-demo-s3.yaml)
- [Separate S3 port](https://github.com/jichaowang02-lang/macless-haystack-esp32s3/tree/bb1ac6e8c022f39b8560fe95bc8a46da8c0695aa)
- [Inspected macless key generation](https://github.com/dchristl/macless-haystack/blob/0bda271b8bd0cc37194dfba1845cd48954cba4cc/generate_keys.py)
- [Apple accessory developer requirements](https://developer.apple.com/find-my/)

## Licensing

Crosscub's root license is MIT. Original OpenHaystack and inspected macless/IDF
firmware derivatives are AGPL-3.0; FindMy.py is MIT. The inspected ESPHome external
repository has no detected root license. Review file-level provenance and
obligations before reuse; small copied snippets do not escape licensing.
Protocol facts can inform independently written code, but this is not legal
clearance or a claim of a formal clean-room process. No upstream firmware is
imported by this research.

## Recommended implementation sequence — not performed

1. Small native NimBLE component, explicit opt-in/off by default, visible state,
   immediate stop, public-only provisioning and reviewed privacy/key lifecycle.
2. Early BT retention and powered-on advertising; pause for AWDL; orderly sleep
   shutdown. Do not change current sleep semantics in the initial prototype.
3. Exact target build, OTA/RAM checks and independent RF capture using synthetic
   or explicitly provisioned owner material. Verify opt-out and restart behavior.
4. Owner-authorized collector authentication and actual Apple-relayed/decrypted
   report, preserving original timestamp. Packet formatting alone is not success.
5. Only then consider tracking standby or timer-wake bursts, with measured current
   and explicit reader/input/storage regressions. Do not invent battery life.

If finding a normally sleeping reader is the immediate goal, a genuine separate
Find My tracker avoids keeping the ESP32-S3 awake and preserves current firmware
sleep. It is a hardware alternative, not an implemented Crosscub feature.

## Retained evidence

`/root/.hermes/task-state/crosscub-findmy-research/`:

- `hardware-audit.json`: exact source, startup, SDK/cache, power and radio evidence.
- `ecosystem-audit.json`: pinned upstream sources, security and license findings.
- `published-image-budget.json`: release image and OTA headroom.
- `offline-probe.log`: executed synthetic probe output.
- `CURRENT.json`: research checkout identity and scope.

Unverified: candidate compilation/linking, actual beacon emission, Apple relay,
owner report retrieval, coexistence, opt-in UI, sleep/wake regressions and power.
