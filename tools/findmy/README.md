# Crosscub private Find My preparation (offline only)

This tool **does not register with Apple**, log in, recover account/keychain
material, pair over BLE, or implement Play Sound. It prepares a private SD input
for an experimental account-side registration project, not Add Other Item pairing.
All capability flags refer to this host tool, not the separate firmware work.
`status` is a local capability report, never a cloud/account status check.

## Commands (Python standard library, Linux/POSIX)

Run from the checkout; flags/order are deliberately fixed. No secret-valued flags,
environment credentials, stdin, stdout export, subprocesses, network, dependency
installation, or implicit writes are supported. Unknown flags fail without echo.

```
python3 tools/findmy/crosscub_findmy.py status
python3 tools/findmy/crosscub_findmy.py dry-run
python3 tools/findmy/crosscub_findmy.py validate --input /PRIVATE/input.json
python3 tools/findmy/crosscub_findmy.py prepare --input /PRIVATE/input.json --output /PRIVATE/crosscub-findmy.json --confirm-private-export
python3 tools/findmy/crosscub_findmy.py import-upstream --input /PRIVATE/tracker.private.json --output /PRIVATE/crosscub-findmy.json --confirm-private-export --confirm-own-tracker
```

`dry-run` is a no-input, no-key capability/plan check; it does not validate a file.
`validate` reads only, prints no identities/secrets, and does not prove registration.
`prepare` validates an existing Crosscub-format private file and creates a new
transfer file. `import-upstream` converts only an owner's newly created tracker
from the pinned upstream's `tracker.private.json` data format; do not use existing
AirTag/account credentials, cloned identities or third-party tracker files.
The ownership flag is an explicit operator assertion, not proof of ownership.
All failures are fixed error codes (exit 2), never exception text or tracebacks.

Input must be an absolute, canonical-component, non-symlink path outside every
Git checkout, with a regular file owned by the current UID, mode exactly 0600,
one hard link, <=4096 bytes. Output must be outside every Git checkout, in an
existing UID-owned mode-0700 directory. No `..`, `.`, repeated separators,
symlinked ancestors, overwrite, or stdout destination. The output is staged 0600,
fsynced, hard-link-published atomically with collision refusal, then staging is
removed and directory fsynced. A filesystem must support hard links and fsync;
unsupported storage fails closed. A late fsync error can leave a complete output
with an uncertain durability outcome: inspect locally, never blindly retry or
assume a failure means no file exists. Sudden process death may leave a private
`.crosscub-*` staging file; do not publish it or make it world-readable.
Same-UID malicious processes and an administrator remain outside this boundary.

## Exact firmware handoff proposal: `crosscub-findmy-v1`

JSON object with **exactly** these four string fields (no sample private keys in
this document):

- `schema`: literal `crosscub-findmy-v1`
- `master_private_key_hex`: exactly 56 lowercase hex characters, 28-byte big-endian
  secp224r1 **private scalar**, strictly `0 < d < n`, with
  `n = FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D`.
- `primary_ratchet_seed_hex`: exactly 64 lowercase hex characters, the original
  32-byte **SKN/sharedSecret at pairing**, not an already advanced ratchet state.
- `paired_at`: real UTC timestamp, exactly `YYYY-MM-DDTHH:MM:SSZ`, second precision.

No IDs, passwords, account tokens, secondary seed or cloud records are transferred.
This is a **primary-only beacon input**, not sufficient to register an account.
Firmware must retain pairing epoch and progress monotonically/reconstruct the
900-second schedule across resets; do not reset epoch/index at every boot. Index
zero uses the original seed; advancing to index i applies upstream's update KDF i
times. This CLI intentionally performs no KDF, EC generation or schedule math.
It rejects upstream nonzero alignment indexes, changed alignment dates, malformed
secondary seeds and unexpected fields rather than silently guessing alignment.
`master_key` and `skn` in pinned FindMy JSON map directly to these two hex fields;
its `paired_at` UTC `+00:00` suffix maps to `Z`. This compatibility is source-reviewed
and synthetic-data tested, not exercised through the absent FindMy dependency.

## SD transfer safety

Keep original and transfer files in a private encrypted directory **outside the
repository**. The CLI never creates `device_secrets.h` and never embeds keys in a
firmware image. After parent firmware integration confirms the actual SD path,
transfer the file as `/crosscub-findmy.json` on a dedicated SD card (or use the
firmware's documented path if different). FAT/exFAT does not enforce POSIX 0600:
create on private host storage first, then explicitly copy to an owner-controlled
card with the reader off. Physical SD possession discloses tracking keys. Never
attach keys to issues, serial logs, CI artifacts, screenshots or this repository.
Remove the import file after the firmware has securely persisted it, if that
persistence/erasure workflow is supported. Flash/SD backup retention and encryption
remain owner responsibilities; deleting a flash file is not guaranteed secure erase.
Local file creation or beacon transmission is not evidence of cloud acceptance.

## Remaining gates

`cryptography`, `Crypto`, `findmy` and `pytest` were absent in the assigned Python
runtime. No packages were installed. Key generation/derivation is therefore not
implemented; no handwritten replacement crypto is used. Use separately reviewed
vetted tooling to create a **new owned** identity; do not invent a scalar by hand.
There is deliberately no runnable upstream installer or cloud runner in this
artifact. See SECURITY-REVIEW.md for the audited boundary and remaining review.
A future cloud adapter needs separately reviewed transitive code/dependencies,
explicit operation-specific opt-in, private account isolation, one-attempt recovery,
bounded record ownership/journals and exact read-back. Neither flags nor this
owner authorization bypass those engineering gates. Native Items UI, actual report
acceptance, clock alignment, unwanted-tracking safeguards and Play Sound still
require independent device/account qualification.

## Repeatable host regression entrypoint

```
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s test/findmy/host -v
```

Only synthetic values and temporary private files are used. No firmware build,
Apple service request, real identity or external registration is exercised.
RED/GREEN records are outside Git under
`/root/.hermes/task-state/crosscub-findmy-implementation/host/`.
