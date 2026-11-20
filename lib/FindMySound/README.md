# Local sound-test controller — not native Find My Play Sound

## Implemented boundary

`FindMySound.h` is original, portable C++17 production code, with no allocation,
Arduino dependencies, RF, account access, parser, GATT registration or GPIO.
Default is disabled. It accepts **only trusted local manual testing**, and only
when all driver callbacks exist, the feature is explicitly enabled, and the
trusted `localAuthorized` callback approves. NativeOwner, NonOwner, Unknown and
invalid Source values are rejected; no flag upgrades an incoming phone command
to authenticated ownership. `available()` means a complete adapter was supplied,
not that an Apple sound capability should be advertised or sound was measured.

A valid local request passes a 1–5000 ms duration to the real driver callback.
Repeated starts while active are refused (no deadline extension). `tick()` stops
at the elapsed deadline, including uint32 clock rollover, or upon local policy
revocation. `stop()` is idempotent; disable stops immediately. Failed starts are
not reported as success and trigger defensive stop because hardware may already
be partially energized. Failed stops retain active/uncertain state, latch a fault,
and disable new starts; retry stop until it succeeds, then explicitly re-enable.
Destruction makes a best-effort stop. The controller cannot silence broken hardware.
`Started` means driver acceptance, **not** native phone success or audible proof.

## Adapter contract for parent HAL/UI owner

- Keep UI copy **Local buzzer test**, never Find My Play Sound.
- Install a driver only after actual buzzer initialization succeeds. No adapter
  means unavailable. Never infer lack of buzzer from `hasAudio()==false`.
- Only local foreground/physical controls can call `start(LocalManual, …)`;
  do not expose it to BLE, HTTP, Lua remote controls, or supplied source fields.
  `localAuthorized` must read trusted local state, not a caller Boolean from RF.
- Serialize all methods and callbacks on one owner task; callbacks must be
  bounded, non-reentrant and non-throwing. Context must outlive the controller.
  This is not an ISR API. Copy/move are intentionally prohibited.
- Drive `tick(millis())` regularly; elapsed time uses unsigned uint32 subtraction
  and requires servicing more often than a full clock wrap. On foreground exit,
  disable, sleep, or revoked consent call `setEnabled(false)` synchronously,
  rather than waiting for a later tick. Never queue/replay manual requests.
- Driver start must enforce the requested duration independently, even if the
  foreground loop stalls. A timer-backed adapter can provide that guarantee.
  SDK `Buzzer::tone(freq, duration)` is blocking and stops itself; it cannot give
  an interruptible, nonblocking UI. `tone(freq, 0)` alone is NOT a bounded adapter.
  SDK tone/noTone return void; do not fabricate hardware-success feedback.
- The controller is a bounded policy core, not a firmware-integrated driver.
  Target compilation and on-device audible/stop/power qualification remain open.

## Hardware evidence

Read-only SDK at `/root/github/crosssmudge-browser-backend/freeink-sdk`, HEAD
`b784446302c076b4b5a622627867f0c80905cc50`, exactly matches this firmware base's
`freeink-sdk` gitlink. In `libs/hardware/BoardConfig/include/BoardConfig.h`:

- lines 839–844: `STICKY_AUDIO` is `AudioOutput::None` **and buzzer pin 48**;
- line 1482: actual Sticky board profile selects `STICKY_AUDIO`;
- lines 1492–1494: separate PDM microphone (clock19/data20/enable38).

Thus Sticky is **not microphone-only**: a passive LEDC buzzer exists, but no PCM
speaker/output codec. `libs/hardware/Buzzer/include/Buzzer.h:5–10,28–36` distinguishes
tones from PCM and documents bounded blocking vs continuous tones.
`src/Buzzer.cpp:17–38` initializes from `BoardConfig::ACTIVE.audio.buzzer`, starts
silent, uses LEDC tone, and stops. Pins here are evidence only; no driver guesses
or direct GPIO changes were made. Loudness/compliance is not established.

## Native owner protocol gate

No native owner command parser is implemented or advertised. Public evidence is
insufficient to authenticate a new ESP32 peripheral as the owner's existing
AirTag-profile item, and no validated native phone trace was available. Parsing
known opcodes without authenticated session establishment would create exactly
the arbitrary-nearby-beep endpoint excluded from this prototype.

Pinned VibeCoderLab/esp32-findmy-integration
`1c0ede3989ee11bd97e9bed169a7fbc6a99db460`:
`docs/NATIVE-NEARBY-REVIEW.md` explicitly states sound unsupported;
`firmware/src/main.cpp` sets `ADV_TYPE_NONCONN_IND`, registers only GAP callback,
and has no GATT server, owner authentication or sound handler. Account-side
CloudKit insertion/native Items visibility does not establish a BLE owner bond,
identity resolution, control session, or ordinary Add Other Item pairing.

Public reverse engineering of **AirTag/Durian** describes GATT provisioning and
updates, but normal owner commands on raw **L2CAP**, with sound mutex acquisition,
sound sequence, acknowledgement/status, then release. In seemoo-lab/airtag
`18a5a7d9738bf9f52c57340e6f1a207a320304e3`, README lists Durian Set Mutex=14,
Play Sound Sequence=40, Stop Sound=8, distinct unauthorized sound commands.
Its scripts use an already paired jailbroken iPhone; the WOOT22 paper explicitly
keeps pairing/provisioning/connection establishment intact. This is not a complete
new peripheral authentication implementation. Hawkeye third-party Sound Start=512
and Sound Stop=513 are separately listed, not interchangeable AirTag wire commands.

**Non-owner is not owner:** DULT draft-ietf-dult-accessory-protocol-01 §§3.11,
3.13.4 specifies an unencrypted non-owner GATT service and Sound_Start=0x0300,
Sound_Stop=0x0301 restricted to separated state. Those anti-stalking commands,
including legacy unauthorized AirTag GATT sound, do not implement normal owner
Find My Play Sound. This controller does not expose them; it also does not claim
DULT compliance or replace the safety work needed before production tracking.

Atmosic openair `91ce3973800ee7a5a3db76cef0ea69cf28a0ff42`
`applications/fmna_tag/src/main.c:108–111` registers sound_init/start/stop callbacks;
`src/sound.c:20–39` only controls PWM. Those glue callbacks are not the transport,
owner authentication, request parser, or acknowledgement protocol. The linked ADK
has an MFi-restricted license. No private SDK, binary or glue was copied or used.

Remaining gate: establish legitimate provisioning/session authentication for the
selected AirTag-vs-accessory profile; obtain complete legally usable protocol or
owner-authorized trace evidence, validate identity/bond/security and exact framing,
mutex/reply/failure semantics, then TDD actual bounded parser against that evidence.
A successful synthetic callback test must never be reported as native compatibility.

Sources:
- https://github.com/VibeCoderLab/esp32-findmy-integration/blob/1c0ede3989ee11bd97e9bed169a7fbc6a99db460/docs/NATIVE-NEARBY-REVIEW.md
- https://github.com/VibeCoderLab/esp32-findmy-integration/blob/1c0ede3989ee11bd97e9bed169a7fbc6a99db460/firmware/src/main.cpp
- https://github.com/seemoo-lab/airtag/tree/18a5a7d9738bf9f52c57340e6f1a207a320304e3
- https://raw.githubusercontent.com/seemoo-lab/airtag/main/woot22-paper.pdf
- https://datatracker.ietf.org/doc/html/draft-ietf-dult-accessory-protocol-01
- https://github.com/Atmosic/openair/tree/91ce3973800ee7a5a3db76cef0ea69cf28a0ff42/applications/fmna_tag
- https://developer.apple.com/find-my/

## Offline regression entrypoint

`python3 test/findmy/sound/run.py`

Builds and runs the actual production header with synthetic driver callbacks in
normal and UBSan modes; no downloads, package installs or shared build caches.
Temporary executables go under `$TMPDIR` and are removed. Tested admission,
source denial, all partial callback combinations, revocation, duration limits,
no extension, rollover deadline, explicit stop/disable, failure/uncertainty recovery,
and destruction cleanup. It does not test a radio, physical sound or native iOS.
Top-level `test/CMakeLists.txt` is outside this worker's ownership and requires
network-backed googletest; parent may register this offline entrypoint without
expanding the sparse checkout. No full firmware build was run.
