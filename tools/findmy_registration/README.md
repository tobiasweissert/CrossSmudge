# Crosscub private Find My registration: offline setup + pinned handoff

**Delivered boundary: runnable, credential-free source preparation and tested two-record
transaction adapter. NOT a live Apple account-registration implementation.** The account
entrypoints deliberately reject execution until the transitive runtime is qualified.
Native iOS Items visibility, location reports and native Play Sound acceptance have NOT
been performed. This is not normal Add Other Item pairing, an identity clone or a claim
of MFi certification.

## Run now (no dependencies beyond Python + Linux stdlib)

From the Crosscub checkout:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s test/findmy/registration -v
python3 tools/findmy_registration/registration.py prepare \
  --private-home "$HOME/.local/state/crosscub-findmy-registration"
python3 tools/findmy_registration/registration.py fetch-sources
python3 tools/findmy_registration/registration.py login
python3 tools/findmy_registration/registration.py recover
python3 tools/findmy_registration/registration.py apply
python3 tools/findmy_registration/registration.py remove
```

`prepare` creates/checks an owner-UID, mode **0700** private directory outside any
Git working tree. Symlinks in its ancestry and an existing less-private directory are
refused, not silently repaired. It generates NO keys and reads NO credentials. All four
account commands above are dry-run, with zero account requests. No password/passcode/
code/token argument is accepted. No installer, shell, firmware builder or device-flash
command is executed or generated.

### Reproducible public-source bootstrap (separate opt-in)

```sh
python3 tools/findmy_registration/registration.py fetch-sources \
  --private-home "$HOME/.local/state/crosscub-findmy-registration" \
  --execute --allow-public-source-network
python3 tools/findmy_registration/registration.py verify-sources \
  --private-home "$HOME/.local/state/crosscub-findmy-registration"
```

The first command performs only HTTPS GETs to `raw.githubusercontent.com` for missing
entries in `source.lock.json`. It sends no Apple request, account identity or credential.
It disables environment proxies and redirects, bounds each read to 500,000 bytes and
checks every SHA-256 before writing mode0600 text under `PRIVATE_HOME/sources/`.
Existing files are hash-checked, never overwritten, repaired or executed. An interrupted
partial file requires explicit owner investigation; there is no repair/retry loop.
`verify-sources` is entirely offline. Neither command imports downloaded source.
The locks, certificates and Python files are **review inputs**, not an installed or
approved executable dependency closure. Public source downloads are not Apple binary
or SDK downloads. No package manager runs.

## What is actually usable in the adapter

`registration.py` exports `FindMyAdapter` and `PairJournal` for a future qualified local
runtime. Tests exercise these actual production objects against synthetic transport,
including the exact pinned `AsyncCloudKitClient.record_save` function selected by AST
from a hash-verified MIT source fixture. Whole upstream modules are NOT imported.
The fixture response/record objects are explicitly synthetic, not Apple responses.

The injected contract is:

* `FindMyAdapter(store, complete_records, SaveSemantics.CREATE, zone_tag)`; construction
  performs no I/O. The CREATE enum name/value are checked; UPDATE is refused.
* `complete_records(store)` must return a **complete, from-origin** snapshot, never
  incremental changes or an unqualified empty page. Duplicate or inconsistent IDs fail.
* `await PairJournal(home).prepare([master_record, naming_record], adapter)` persists
  exactly two distinct UUIDv4 IDs, the required ordered record types, hashes binding
  fields/protection/full zone identifier, and a complete baseline fingerprint map.
  These are already constructed records; this code invents no PCS/SRP/CloudKit formats.
* `await journal.apply(records, adapter)` defaults to dry-run and makes no transport call.
  `execute=True` is reserved for synthetic tests or a separately approved runtime.
* Explicit execution locks the installation across all awaits, revalidates the exact
  journal/pair and baseline, refuses collisions and ANY prior attempt, durably checkpoints
  intent **before each** create, and calls the existing pinned `record_save` function with
  explicit CREATE semantics, empty record protection tag and the selected zone tag.
* After each write a complete readback must match the entire expected map, including
  every original baseline record and previously written half of the pair. A timeout,
  exception, unexpected readback or interrupted second write stays `uncertain` with exact
  attempted IDs. No retry, overwrite, new identity, automatic rollback or automatic
  cleanup is attempted. Keep `pair.json` (0600); it contains hashes/IDs, not keys/payloads.

This contract harness does **not** prove real protobuf/crypto authentication, Apple's
record-readability, cross-process account locking or cloud success. The journal locks
only this one installation; CREATE semantics, not a local snapshot, must enforce
server-side no-overwrite against a competing client. A hostile same-UID process that can
edit the private directory is outside the isolation boundary. Do not inject arbitrary
network clients into the API or interpret `execute=True` as closure approval.

### Why remove is intentionally unsupported

The pinned FindMy library exposes record_save, not a reviewed public record-delete
method. Upstream `host/cloud.py` constructs a private delete request with field framing
and automatically invokes cleanup after some uncertain writes. This tool does NOT copy
that framing, invent a replacement, or delete either a tracker or keychain record.
Account `remove --execute` returns `E_REVIEW_REQUIRED`, with no requests. This is safer
than presenting a success-shaped removal stub. Before enabling removal, require a
reviewed conditional-delete primitive, exact owned attempted IDs, full-pair validation,
no foreign-record/renamed-identity adoption and authoritative absent readback. Never
remove escrow records, trust-circle peers or existing accessory identities to recover
from a tracker-registration error.

## Owner-local continuation gates (not an invitation to sign in yet)

See `SECURITY-REVIEW.md` for decisive source findings. Immutable source roots are:

* `sources/candidate/`: VibeCoderLab/esp32-findmy-integration,
  `1c0ede3989ee11bd97e9bed169a7fbc6a99db460`.
* `sources/findmy/`: parawanderer/FindMy.py,
  `7969003ca785658369b650f75d9e7ca519566bf6`.
* `sources/os/`: sacca97/os-findmy-e,
  `374d83854f657a7fdca45f813387d99d491db0e2`.

The source bundle is a pinned **subset for review**, not a runnable upstream checkout.
Do not run `candidate/host/install.py`, `scripts/setup.sh`, or downloaded login/register
modules directly. The setup script installs dependencies, mutates dependencies and
acquires proprietary Apple runtime data; the installer also operates on hardware.

Before a separate owner-local account implementation can be released:

1. Complete and freeze the full executable import/dependency closure. Review the exact
   FindMy wheel and packaging/status patches, Anisette source/artifacts, os-findmy-e
   auth/output/machine-state/cooldown patches, HTTP utility/redirect/size/retry behavior,
   native emulator imports and the exact independently acquired Apple runtime profile.
   Version strings alone are not provenance. This task did not install/acquire them.
2. Use a fresh isolated owner-local environment (upstream lock targets Python **3.12**).
   Upstream FindMy metadata at this commit says **0.10.1**, not 0.10.2. Its requirements
   include cryptography, SRP, aiohttp, protobuf, typing-extensions, bleak, bs4 and Anisette;
   the Mac path additionally needs Unicorn and its reviewed emulator/runtime. The public
   package lock includes `anisette==1.2.4`; its source/native closure is NOT approved here.
   Do not install the broad firmware/PlatformIO lock for registration alone.
3. Provide separately protected owner-interactive login and recovery operations: terminal
   prompts only, no chat/argv/environment secrets, hidden code entry, suppress native FDs,
   encrypted account storage, same-machine restore, fixed error codes, bounded process
   groups, denied-by-default networking and exact Apple endpoint/TLS/redirect policies.
   Checkpoint one recovery intent before the passcode exchange and never automatically
   retry an unknown outcome. Do NOT call keychain join/establish/enrol/delete.
4. Use only installed, source/hash-verified FindMy cryptographic functions. Upstream
   fresh-envelope construction in `host/fresh_protection.py` is experimental protocol
   code, not an approved credential component. Do not reimplement it here or reuse another
   accessory's identity/encryption template. A missing BeaconStore zone is unsupported;
   do not create a zone or reset account state as a fallback.
5. Bind an owner-reviewed candidate to the tested journal and explicitly admitted account
   transport. Reverify source/runtime hashes at launch. Cloud apply must CREATE only this
   fresh MasterBeaconRecord + BeaconNamingRecord and read back both and the baseline.
   Complete a separate reviewed removal mechanism before offering live writes.
6. Only then invite the owner to approve exactly one login, recovery and apply. Confirm
   real iOS Items visibility independently; qualify actual relayed location and app-native
   Play Sound separately. A readback or a visible button is not either acceptance.

`login|recover|apply|remove --execute --allow-account-network --confirm-own-tracker`
currently fails closed. Those flags do not bypass the unresolved gates. There is no
secret-reading, credential-taking or live-import escape hatch in this CLI.

## Device handoff boundary

No device configuration or firmware source is written here. The later approved export
must pass only the Crosscub-owned master scalar (28 bytes), **original** SKN (32 bytes)
and original pairing time into the parent's four-field schema, starting the core's
initial index at 0. Do not transfer Apple credentials, recovered keychain/PCS material,
secondary SKS or another accessory's identity. Keep that separate private artifact out
of Git and preserve the cloud's original pairing metadata. This task generated neither
real nor substitute keys and performed no flashing.
