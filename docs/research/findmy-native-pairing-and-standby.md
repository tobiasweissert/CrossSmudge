# Crosscub: native Apple Find My pairing and tracking standby

Status: feasibility follow-up complete. No implementation or device qualification.

## Owner requirements and scope

- Keep tracking available while the reader appears asleep, not powered-on-only.
- Add the reader as a new item in Apple's native Find My app through legitimate
  accessory pairing; a separate location collector/map does not meet this goal.
- OTA changes are handled in another workstream and are not a blocker here.
- Research only: no firmware edits, builds, installation, flash, real keys,
  provisioning, Apple login/service calls, RF transmission, commit or push.

Baseline: firmware `818b0befac4d9249c7abded67fa980187c4b41af`, SDK
`b784446302c076b4b5a622627867f0c80905cc50`. This follow-up supersedes the initial
report's powered-on-only prototype recommendation for the owner's desired product.

## Private-project clarification and reported native-app success

The owner clarified this is a private project, not intended for sale, and asked
whether hobbyists have actually made their own tracker appear in Apple's app.
Normal Add Other Item pairing and native Items visibility are different goals;
the official MFi route must not be presented as the only possible private
experiment merely because it is the supported new-accessory pairing route.

**A concrete positive author report exists:**
[VibeCoderLab/esp32-findmy-integration](https://github.com/VibeCoderLab/esp32-findmy-integration/tree/1c0ede3989ee11bd97e9bed169a7fbc6a99db460)
reports a classic ESP32 displayed in Apple's native Find My Items with location,
editable name and emoji. Its FRESH-PROTECTION and release notes describe both an
initial experiment and a fresh installation on the same account. It generates
new tracker keys and identifiers rather than requiring a physical donor AirTag.

Its approach is authenticated account-side creation of encrypted CloudKit
MasterBeacon/Naming records, combined with an on-device rolling-key beacon. It
is NOT nearby Add Other Item pairing. Author-reported native app success makes
it a serious private-project feasibility candidate, not a proven Crosscub port.

The parent re-fetched the exact README, DESIGN, FRESH-PROTECTION, release notes,
firmware config, current commit metadata and public issues. Current main remains
`1c0ede3989ee11bd97e9bed169a7fbc6a99db460`. GitHub showed no public issues or forks;
no independent second-account success was established. The authors explicitly
state second-account/empty-account and S3 validation are still missing. There
are no screenshots/images in the inspected tree; native UI success remains a
written author report, not an independently reproduced result.

Limits: existing recoverable iCloud Keychain/BeaconStore state is required;
setup handles account credentials and a device unlock passcode and writes
private account records. These operations need separate security review and
explicit authorization, not execution of the one-command installer. No UWB,
nearby search or sound; battery is a placeholder. On-device rolling material
includes private secrets, unlike the initial public-only static-key probe.
Power-off key-schedule continuity/alignment remains a porting concern. Apple
can change these private interfaces. No account access or tool execution occurred.

**Revised private-use assessment:** native-app tracking has a concrete reported
DIY route worth evaluating for Crosscub. Normal accessory pairing on ESP32-S3
remains unproven, but lack of MFi access alone is not a reason to dismiss this
separate account-registration experiment.

## Overall decision

**Sleep redesign: source-level feasible on the existing ESP32-S3, subject to
hardware/current/regression qualification. Native pairing: plausible in principle,
but not established as an available firmware-only upgrade for this reader. Native
Items display through experimental account-side registration has a concrete
positive author report, with the limitations above.**

For supported native accessory pairing, the missing gate is legitimate Find My
provisioning and the full licensed protocol/platform assessment, not OTA. For the
private account-registration experiment, the gates instead are source/security
review, account compatibility, safe targeted record lifecycle and an S3 port.
A BLE beacon, native Items visibility and Add Other Item pairing are distinct.

## Tracking standby instead of system deep sleep

Recommended conceptual state machine:

`ACTIVE -> PREPARE_TRACKING_STANDBY -> TRACKING_STANDBY -> RESTORE_ACTIVE`

Keep a separate explicit deep-sleep/off state, honestly labeled tracking unavailable.

During tracking standby, retain MCU supply, RAM/PSRAM and the Bluetooth
controller/NimBLE service. The e-paper image can remain visible with its panel
rail off; touch, SD and other peripheral rails can be parked after their users
have stopped. Keep a controller advertising procedure enabled and let it schedule
BLE events. Use Bluetooth modem sleep plus PM-driven automatic light sleep
between events, rather than calling manual light/deep sleep from the UI loop.

### Supported clock/power route

Espressif's actual S3 NimBLE power-save profile supports the main 40 MHz crystal
remaining powered during automatic light sleep:

- `CONFIG_BT_CTRL_MODEM_SLEEP=y`
- `CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y`
- `CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y`
- `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y`
- PM and FreeRTOS tickless idle, explicit `esp_pm_configure` and appropriate locks.

The official Seeed schematic identifies a main 40 MHz crystal. Its separate
PCF8563 RTC's 32.768 kHz crystal is NOT proof of a clock wired to the S3. GPIO15/16
are already assigned to display CS/DC; do not assume an external-S3-32k profile.
Actual owner board revision/population and runtime clock remain unverified.

The current cached framework disables PM and controller modem sleep. These are
IDF component settings requiring an isolated, consistent SDK regeneration later;
application-only defines cannot repair precompiled archives. Retain Bluetooth
memory through the early Arduino `btInUse` decision before `setup()`.

### Real lifecycle work, not just removing one sleep call

Current `enterDeepSleep()` destroys/replaces activities, shuts storage, prepares
a sleep frame and enters reset-style sleep. Simply deleting its final sleep call
would leave the running application in an invalid lifecycle state.

A real standby path must:

1. Stop accepting new work, drain rendering/storage and join background jobs
   before power-cycling the devices those jobs use. Do not suspend lock owners.
2. Replace manual CPU-frequency changes with coordinated PM policy; block idle
   tasks without blocking Bluetooth controller/host/timer work.
3. Preserve or checkpoint/reconstruct activity state explicitly. RAM retention
   does not preserve valid SD handles after SD power cycling.
4. Keep main supply latches asserted; order display, shared SPI/SD bus and rail
   parking/restoration safely. Do not blindly apply deep-sleep GPIO isolation.
5. Resume in the same boot using validated power-button wake and release/debounce
   suppression. Current deep-sleep reset classification is not a standby wake API.
   Powering down touch means button wake, not touch wake.

Source-level fallback: keep the MCU awake in screen-off standby with a suitable
power policy if automatic light sleep cannot be qualified. It may use more power;
no device current or battery-life measurements exist for either route.

### Meaning of always available

Scheduled BLE advertising is discrete packets, not continuous RF or guaranteed
fresh locations. Nearby participating Apple devices still have to hear and relay
an observation. Resets, empty battery and true electrical off cause gaps.
Timer-wake/deep-sleep bursts add deliberate discovery gaps and do not meet the
same continuously scheduled beacon goal.

The initial conservative BLE pause for AWDL/AirDrop also creates a gap. A later
implementation must qualify non-pausing coexistence, restrict conflicting radio
use, or disclose the gap. A separate tracking radio can avoid sharing the S3 RF
lifecycle, but is additional hardware and needs its own power/safety design.

## Native Apple Find My: what pairing actually needs

OpenHaystack advertisements do not supply native owner registration. Generic BLE
bonding is not Find My ownership. The native route includes accessory
discoverability, pairing/control services, authenticated provisioning, key/state
management, persistent ownership, removal/reset/re-pairing and required privacy
and unwanted-tracking behavior. Exact current requirements are program-gated.

### Strong evidence that legitimate native pairing exists

- Apple directs developers of new OR existing accessories to the MFi program.
- Nordic's pinned integration documentation places its Find My SDK/documentation
  in a private repository accessible to MFi licensees. Its public support for
  Nordic SoCs is not ESP32 portability evidence.
- Atmosic's public native-tag evaluation documentation explicitly describes
  adding a properly provisioned EVK using the Find My app. Authentication UUID
  and token values come from MFi. The downloadable ADK binary has an MFi-restricted
  license, not an unrestricted open-source license.
- InPlay publicly documents Apple-issued software-authentication tokens and UUIDs,
  including renewal during pairing. Therefore it is WRONG to claim that every
  Find My accessory necessarily needs UWB or a separate authentication chip.
  Software tokens still cannot be self-issued.

These establish a legitimate vendor/SDK route, not permission or provisioning for
an arbitrary ESP32-S3 port. Whether the current ESP32 platform and product category
meet the confidential security/safety requirements remains unknown.

### Development is not production approval

A licensed provisioned evaluation unit can demonstrate native pairing. Public
sources do not settle arbitrary-account/profile/allowlist restrictions for every
vendor's development tokens. Vendor confirmation is needed before promising
ordinary-iPhone evaluation pairing.

Likewise a working demo is not certified finished-product compliance. For example,
Atmosic's demo can substitute an LED for a buzzer and omits motion sensing.
Determine the reader category's exact sound/motion/disable/reset obligations
from current specifications; do not assume tag requirements or exemptions.
Clearing local firmware data is not proof that an Apple Account association was
removed. Preserve credentials securely; vendor debug token logging must not be
carried into a product or research logs with real provisioning.

### Unofficial alternatives checked

No verified public ESP32/S3 implementation of NEW Add Other Item pairing was found
in the targeted search. This is not proof that one cannot exist.

- `VibeCoderLab/esp32-findmy-integration` claims native Items display on one account
  through account-side CloudKit record insertion. That is not Add Other Item
  firmware pairing; independent/different-account validation remains unproven.
- `Thomas-Bringolf/findmy-esp32-s3` explicitly has no GATT/MFi and provisions keys
  over USB. Deriving rolling keys does not turn it into native pairing.
- Existing-tag identity replay is not a new accessory identity or reliable pairing
  implementation. No cloning, credential extraction or account modifications were
  attempted or proposed here.

## Practical feasibility routes

1. **Existing S3 + licensed full accessory implementation:** conditional. Obtain
   authorized specification/provisioning access and explicit platform/security
   portability assessment, then qualify tracking standby. Not proven available now.
2. **Supplier-supported Find My companion hardware:** credible development route,
   potentially decoupling reader sleep and RF contention. Confirm exact firmware,
   legitimate provisioning, final-host approval and ordinary-account evaluation.
   A generic BLE or RF-certified module is not automatically an Apple-approved,
   preprovisioned Find My accessory. No drop-in module satisfying all those claims
   was independently established in this research.
3. **Complete certified Find My accessory in/on the case:** the most direct way to
   meet native-app pairing while leaving reader deep sleep intact. This is a
   separate accessory, not a firmware integration.

The next feasibility gate for the licensed route is a supplier/program answer on
provisioning and ESP32-S3 support. For the owner's private project, the experimental
account-registration route deserves a separate source/security/portability audit;
it is not dismissed merely for lacking normal accessory pairing. Neither route
authorizes running an installer, recovering keys or changing the account yet.
No supplier inquiry, enrollment, account registration or submission was sent.

## Sources and retained evidence

- [Espressif S3 40 MHz power-save configuration](https://github.com/espressif/esp-idf/blob/v5.5/examples/bluetooth/nimble/power_save/sdkconfig.40m.esp32s3)
- [NimBLE power-save example](https://github.com/espressif/esp-idf/tree/v5.5/examples/bluetooth/nimble/power_save)
- [Official Seeed schematic](https://files.seeedstudio.com/wiki/reterminal_sticky/res/reTerminal_Sticky_Schematic_diagram_260609.pdf)
- [Apple Find My developer entry](https://developer.apple.com/find-my/)
- [Apple program process](https://mfi.apple.com/en/how-it-works)
- [Nordic licensed integration](https://github.com/nrfconnect/sdk-nrf/blob/d8887f6f32df9e958f393d3490f00b5c4ef88601/doc/nrf/integrations.rst)
- [Atmosic native-tag evaluation documentation](https://github.com/Atmosic/openair/blob/91ce3973800ee7a5a3db76cef0ea69cf28a0ff42/applications/fmna_tag/README.rst)
- [InPlay software-authentication guide](https://inplay-inc.github.io/docs/solutions/find-my/find-my-sdk-guide.html)

Detailed source/file-line evidence, uncertainty and pinned unofficial projects:
`/root/.hermes/task-state/crosscub-findmy-research/sleep-followup.json` and
`native-pairing-followup.json`. Parent independently retrieved the S3 40 MHz
configuration, Atmosic provisioning documentation and InPlay guide. This remains
source/documentation evidence, not a build, pairing or power test.
