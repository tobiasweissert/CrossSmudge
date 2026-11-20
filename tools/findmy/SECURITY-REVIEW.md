# Pinned-source security / provenance review

Candidate: [VibeCoderLab/esp32-findmy-integration](https://github.com/VibeCoderLab/esp32-findmy-integration/tree/1c0ede3989ee11bd97e9bed169a7fbc6a99db460),
exact commit `1c0ede3989ee11bd97e9bed169a7fbc6a99db460`.
License inspected at that commit: **MIT**, copyright (c) 2026 ESP32 Find My
contributors. This host CLI is new implementation of a data conversion/safety
boundary, not vendored upstream registration code. No upstream Python was
imported or executed. Public source snapshots and computed SHA-256 values are
recorded in `upstream-review.json`; snapshots are retained outside Git as review
evidence. The hashes identify the inspected bytes, not an independent signature
or blanket supply-chain certification.

## Source evidence and decision

- `host/records.py:21-49,82-96`: vetted `cryptography` generates secp224r1 keys;
  private scalar is 28 bytes, primary `sharedSecret` and secondary `sharedSecret2`
  are each 32 bytes, epoch is UTC whole seconds. `check_candidate` verifies
  decoded lengths and synthetic advertisement diversity. Public+scalar storage
  in cloud privateKey is **not** the 28-byte firmware scalar representation.
- `host/key_header.py:9-21,37-45`: firmware header contains the 28-byte master and
  32-byte ratchet seed, advanced from `accessory.skn` by 900-second slots. It
  writes private keys under firmware/include/device_secrets.h. **Do not reuse**
  this output path or embed keys in Crosscub firmware/Git.
- `host/register.py:21-64`: misleadingly named `prepare` is not offline: it restores
  account state, prompts password and device passcode, opens keychain sessions,
  enumerates recovery peers and recovers service keys. Its one-attempt pending
  checkpoint is useful, but this whole path remains disabled in Crosscub.
- `host/register.py:65-119`: reads account records, unwraps BeaconStore protections,
  fresh-envelope or template mode, and stores private tracker and registration
  journal. Existing recoverable account/zone material is required. Template mode
  touches existing accessory templates; Crosscub does not implement it. Neither
  mode is normal BLE pairing. Default fresh mode does not eliminate account access.
- `host/cloud.py:20-31,71-155`: manifest/digests constrain newly selected record IDs;
  writes use CREATE, checkpoint attempted IDs before sending, read records back,
  compare the baseline and target cleanup. This is good source-level boundedness,
  not evidence these private APIs work on the owner's account. There is still
  multi-request uncertain-outcome and cleanup behavior to review transitive to
  `snapshot`, `service_limits`, protobuf decoding, auth and HTTP consumers.
- `host/common.py:8-19,36-63`: account state defaults under its checkout; directory
  resolution follows symlinks; writes use os.replace (overwrites collisions).
  `account()` imports login recovery implementation. These are not suitable
  defaults for this Crosscub boundary and were not imported/reused.
- `host/install_findmy.py:39-68` / `sources.lock.json`: pins FindMy.py archive
  `7969003ca785658369b650f75d9e7ca519566bf6`, archive SHA-256
  `efb082865de10feeab1896ceeb6a93a4dfd5b7810769b8a25e7f2940fc2ea575`,
  checks archive entries, applies packaging/HTTP patches, builds wheel and invokes
  pip/uv. **Never executed.** Archive checksum is upstream-declared, not independently
  downloaded/verified here; complete transitive auth/native binaries review is
  still missing. `requirements.lock` was fetched for provenance, not installed
  or certified safe based on its hashes alone.
- Pinned FindMy.py `findmy/accessory.py:554-597` confirms private JSON uses lowercase
  hex `master_key`, `skn`, `sks`, ISO pairing date and alignment fields.
  `:728-752` confirms initial seed at index zero and update KDF per index.
  `findmy/cloudkit/beacons.py` was also fetched for field mapping inspection.
  FindMy.py is **not** bundled, imported or linked; its separate license and all
  native/auth helper licenses must be checked before any future redistribution.

## Remaining live gates (all unqualified)

1. Full dependency/patch/auth/native-helper review and isolated runtime with vetted
   crypto installed; no dynamic downloader or one-command installer shortcut.
2. Explicit separately reviewed account login/recovery operation, one attempt,
   hidden credentials, no saved password/passcode and redacted allowlisted errors.
3. New owned identity generation; no existing tracker identity copying. Safe
   private seed export as the schema, never firmware header or log data.
4. Record create/remove intent journals, permission isolation, complete snapshots,
   bounded exact-ID readback and conservative unknown-outcome recovery.
5. Native app visibility and fresh actual Find My report acceptance on target
   firmware/account; source inspection and offline tests prove neither.
6. Clock continuity/seed advancement on device, physical standby qualification,
   privacy/unwanted-tracking and separately explored authenticated sound control.

**Decision:** implement only audited offline data preparation now. No cloud runner
or vendored registration code is enabled. Registration readiness remains blocked,
not falsely simulated. All source retrieval was public GitHub HTTPS, never Apple
or existing local account/airtag-map data.
