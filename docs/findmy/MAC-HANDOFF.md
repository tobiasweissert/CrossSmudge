# Crosscub – Find My prototype: Mac handoff

**Status: tested source prototype, not a working end-to-end Find My accessory.**

Branch: `research/findmy-tracking` in `tobiasweissert/Crosscub`.
Base: `818b0befac4d9249c7abded67fa980187c4b41af` (Crosscub 1.6.5.5).
SDK gitlink: `b784446302c076b4b5a622627867f0c80905cc50`.
The owner chose to continue on an Apple Silicon Mac. This is a private personal
project, not a product being prepared for sale. Native Apple Find My **Items**
visibility is the goal; a separate OpenHaystack map does not satisfy it.

## Read this first

- No Apple account was accessed or modified. No real identity was generated,
  no BLE radio was activated and no device was flashed in this implementation.
- Nothing is merged into main or published as firmware. This branch preserves
  the experiment. Do not treat it as a ready update for an unattended reader.
- OTA/partition work belongs to another stream. Do not remove image-size checks,
  change OTA admission or merge unrelated OTA code just to make this branch build.
- The default build remains `sticky`. The opt-in development target is
  **`sticky-findmy`**. The normal target does not activate the experiment.
- Source and tests contain deliberately synthetic test values, not owner keys.
  Keep all future tracker/account material outside every Git checkout.

## What exists

1. **Rolling-key core** (`lib/FindMy`): real SHA-256/X9.63/P-224 derivation,
   primary ratchet, 900-second slots, 31-byte advertisement packing, trusted-time
   admission, checkpoint ordering and failure handling. Real public test vectors
   match independent derivation and the pinned FindMy.py implementation.
2. **Native BLE transport** (`lib/HalFindMyBle`): native NimBLE, explicit opt-in,
   asynchronous host synchronization/reset handling, finite leases, exact MAC
   byte order, stop/idle reporting and feature-gated early BT-memory retention.
3. **Runtime** (`lib/FindMyRuntime`, `src/findmy`): private SD import, explicit
   fresh/existing consent, versioned NVS intent/checkpoint with readback, polling,
   frame expiry/renewal, error state and AirDrop/deep-sleep handoff gates.
4. **Settings UI**: Settings → System → **Find My (experimental)**, with separate
   fresh/existing enable actions requiring two selections and an explicit Stop.
5. **Tracking lock**: a healthy opted-in runtime retains the activity behind
   Quick Lock instead of executing that sleep request's destructive teardown.
6. **Offline host preparation** (`tools/findmy`): validates/converts a private
   provisioning file, refuses unsafe paths/overwrites and never prints keys.
7. **Registration groundwork** (`tools/findmy_registration`): public source pins,
   offline preparation, two-record CREATE-only journal/readback adapter and
   synthetic transport tests. This is NOT a released live account client.
8. **Sound-control foundation** (`lib/FindMySound`): bounded, authorized local-test
   controller. It rejects native-owner/non-owner remote sources. It is not wired
   to a working iOS command or claimed as an audible device test.

## What is still missing

### Native iOS registration — blocked in software, not just waiting for a login

The upstream private experiment reports native Items location/name/emoji success
on one account using **new encrypted CloudKit records plus a rolling beacon**.
It is not Add Other Item Bluetooth pairing. Different-account and S3 success
are not established by those author reports.

Candidate source: `VibeCoderLab/esp32-findmy-integration` at
`1c0ede3989ee11bd97e9bed169a7fbc6a99db460`.

Our live `login`, `recover`, `apply`, `remove` entrypoints remain dry-run by
 default; `--execute` returns **`E_REVIEW_REQUIRED`**. These are intentional
incomplete boundaries, not a credentials problem or a successful registration.

Before enabling them, finish the exact dependency/runtime review and hardening:

- Upstream `prepare` is NOT offline: it performs account/keychain recovery.
- Saved login/provider paths can retain plaintext tokens or expose child stderr.
- The emulator can execute unimplemented imports; apparent successful output
  does not qualify real authentication semantics.
- Anisette can acquire native runtime dependencies lazily. Review the complete
  executable closure, exact source/wheel hashes and Apple runtime profile.
- Fresh PCS envelope construction and conditional owned-record removal need
  qualification. Do not use a guessed delete request or automatic cleanup after
  an uncertain write. Preserve the journal and recover read-only.
- Only the new tracker's two records may be created. Never alter existing
  accessories, keychain trust membership, escrow records or the BeaconStore zone.

See [registration README](../../tools/findmy_registration/README.md) and
[security review](../../tools/findmy_registration/SECURITY-REVIEW.md).
Do not run the upstream all-in-one installer as a shortcut. Do not supply Apple
passwords, 2FA codes, device passcodes or exported keychain material in chat.

### Native Play Sound — not implemented

The Sticky has a passive buzzer on GPIO48 in the pinned SDK; the microphone is
separate. The hardware is not the main unknown. The authenticated phone-to-device
owner command path is missing. The current native BLE transport is a
**nonconnectable broadcaster**, not an AirTag owner L2CAP/control implementation.
A licensed vendor's sound callback or a non-owner unwanted-tracking GATT command
is not a substitute for that owner protocol. Do not relabel local PWM as native
Find My sound, and do not add an unauthenticated beep endpoint.

See [sound README](../../lib/FindMySound/README.md).

### Always available / sleep — first slice only

Current tracking lock keeps firmware/RAM alive; it is NOT qualified panel/SD
rail-off or efficient automatic light sleep. Battery/current are unmeasured.
After every reboot it requires NTP this boot and explicit activation. Stop,
failed enable and AirDrop handoff latch it off until a later reboot/consent.
The core deliberately refuses a same-slot restart; a later slot is needed.
These conservative prototype limitations do not meet set-and-forget always-on.

A later power redesign should use the supported S3 main-crystal/modem-sleep/PM
route, preserve driver/activity lifecycles and measure whole-device current.
Do not simply remove `esp_deep_sleep_start()` from existing teardown code.
Actual deep sleep, empty battery, resets and radio contention create gaps;
BLE availability never guarantees that Apple devices relay a fresh location.

## Get the branch on the Mac

Use a clean worktree or checkout, not an existing dirty main checkout. One simple
fresh-clone route (run on the Mac, not the server):

```sh
mkdir -p "$HOME/github"
git clone --branch research/findmy-tracking \
  https://github.com/tobiasweissert/Crosscub.git \
  "$HOME/github/Crosscub-Find-My"
cd "$HOME/github/Crosscub-Find-My"
git submodule update --init --recursive
git status --short --branch
git rev-parse HEAD
git -C freeink-sdk rev-parse HEAD
```

Use the committed SDK gitlink, not `git submodule update --remote` or an arbitrary
latest SDK. This server used a sparse checkout plus an existing SDK read-only;
the Mac should have the full branch and initialized submodules.

## First full firmware build on the Mac

The server did **not** complete SDK regeneration, a firmware build/link, or an
ELF/link-map check. Local project space was critically low. The commands below
are a continuation recipe derived from the existing CI/toolchain pins, not a
claim that the Mac build has already passed.

Install/use `uv` on the Mac, then from the clean checkout:

```sh
uv venv --python 3.14 .venv-findmy
source .venv-findmy/bin/activate
uv pip install https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
export PLATFORMIO_CORE_DIR="$HOME/Library/Caches/crosscub-findmy/platformio"
mkdir -p "$PLATFORMIO_CORE_DIR"
set -o pipefail
pio run -e sticky-findmy -j 1 2>&1 | tee "$HOME/Library/Caches/crosscub-findmy/firmware-build.log"
```

The isolated PlatformIO core directory avoids changing another firmware stream's
packages. Allow substantial disk space for dependencies and SDK regeneration.
Do not assume official registry PlatformIO is interchangeable with the pinned
pioarduino Core fork. The platform is pinned to pioarduino **55.03.37** in the
project; the cached server headers used for syntax evidence identify IDF 5.5.2.

Check the *generated* SDK configuration and final link map, not just app defines:
NimBLE/broadcaster/controller, PM/tickless idle, controller modem sleep, main XTAL
sleep clock, mbedTLS secp224r1/SHA256, and strong `btInUse()` selection/retention
anchor. Check internal heap under reader/network load and final image size.
No OTA size-check bypass is part of this recipe. Do not run `-t upload` yet.

## Tests and portability

Server command actually executed successfully:

```sh
python3 -B test/findmy/run_all.py --sdk --output /PRIVATE/EXTERNAL/OUTPUT
```

Eight offline groups and three target-syntax groups passed; see
[portable evidence summary](VERIFICATION.json). The ordinary Crosscub routine
also registers the feature as `--findmy-only`.

**Known Mac test-harness work:** several transport/runtime/aggregate runners
still default to server `/root/.hermes/...` paths, and SDK syntax runners discover
an existing server cache by an absolute path. Port those output/cache discovery
settings before invoking the full suite on macOS. Do not create `/root` on the
Mac or silently skip failed groups. No macOS execution is claimed here.

The host-only stdlib tests can be tried directly:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s test/findmy/host -v
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s test/findmy/registration -v
```

The core runner supports `FINDMY_TEST_OUT`. With compiler/OpenSSL development
headers installed and discoverable on the Mac:

```sh
export FINDMY_TEST_OUT="$HOME/Library/Caches/crosscub-findmy/core-tests"
mkdir -p "$FINDMY_TEST_OUT"
python3 -B test/findmy/core/run.py
```

The runner currently invokes `g++` and `-lcrypto`; Apple's compiler/OpenSSL include
and library discovery may require adaptation. Do not confuse a harness setup
error with a cryptographic vector failure.

## Provisioning and eventual device test

Only after the account/client security gates and a successful complete build:

1. Create a **new owned** tracker identity using reviewed crypto/account tooling.
   Do not copy an existing AirTag identity or third-party keys.
2. Confirm the new native Items entry independently. A CloudKit response echo or
   readback is not proof that Apple's app can decrypt/display it.
3. Export only the new master scalar, original SKN and original pairing time via
   the strict four-field host contract. No secondary/account secrets on the SD.
4. Put private `/crosscub-findmy.json` on owner-controlled SD; no keys in source,
   headers, build flags, screenshots, logs or Git. Physical SD/NVS access can
   disclose secrets: neither is claimed encrypted/anti-rollback protected.
5. Explicitly flash the correctly qualified experimental application, sync NTP
   and use Settings → System → Find My (experimental), selecting Fresh provision
   twice only for genuinely new state; otherwise Existing checkpoint twice.
6. Verify stop/consent/error state, over-air MAC/payload/rotation and expiry,
   Quick Lock/wake, AirDrop handoff, deep sleep, power loss and report age.
7. Verify actual Apple relaying/location in native Find My separately. Native
   sound remains a separate protocol implementation, not an acceptance checkbox
   that can currently pass.

No manual erase/reset of intent/checkpoints is a supported retry procedure.
Keep uncertain state and investigate rather than generating a new identity.

## Other documentation / evidence

- [Original research](../research/findmy-tracking.md)
- [Sleep/native-app investigation and private-project clarification](../research/findmy-native-pairing-and-standby.md)
- [Core contract](../../lib/FindMy/README.md)
- [Transport lifecycle](../../lib/HalFindMyBle/README.md)
- [Runtime flow and persistence](../../lib/FindMyRuntime/README.md)
- [Offline provisioning schema and CLI](../../tools/findmy/README.md)

Linux tests and independent review cover the frozen candidate source, not Apple,
physical hardware, firmware linkage, visual UI or battery performance. See the
verification summary for the exact scope. Preserve this branch while unfinished;
continuing on the Mac does not authorize a release or a main-branch merge.
