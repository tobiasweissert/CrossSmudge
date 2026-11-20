# Portable experimental Find My rolling-key core

This component derives real P-224 primary rolling keys, not a static OpenHaystack
key or finite key list. It has no radio, Arduino, filesystem, settings, logging,
account, network, or BLE-global dependencies. It does **not** implement account
registration, native GATT pairing, Play Sound, secondary/nearby keys, UWB, or
Apple certification. Compatibility of the key schedule is proven against pinned
software; actual iOS Items recognition/relay remains a separate hardware gate.

## Exact provisioning / parent integration contract

Include `FindMyCore.h`; namespace `findmy`. Compile both `.cpp` files.

```cpp
Provision p;
Status s = Provision::parse(masterBytes, masterSize, seedBytes, seedSize,
                            initialIndex, epochSeconds, p);
Core core(csprngCallback, rngContext); // mbedTLS requires callback
s = core.init(p, durableStore, Time{unixSeconds, monotonicMs, timeIsTrusted}, false);
s = core.rotate(Time{unixSeconds, monotonicMs, timeIsTrusted});
Frame f;
if (s == Status::Ok && core.currentFrame(f)) { /* HAL may publish */ }
auto publicOnly = core.diagnostics();
```

- `masterBytes`: **exactly 28** bytes, unsigned big-endian P-224 scalar with
  `1 <= d < N`. `seedBytes`: **exactly 32** bytes of primary ratchet seed. Null,
  wrong length, zero scalar, out-of-range scalar, or zero epoch fail closed.
  Length alone does not distinguish a private scalar from a public x coordinate.
  Use an explicit private provisioning schema; never infer type by length.
- `initialIndex`: the index **already corresponding to the supplied seed**.
  No implicit initial ratchet step. `epochSeconds`: Unix start time of that index's
  900-second slot, **not** import time. Host writer must export an aligned
  seed/index/epoch tuple from registration. Do not pass unadvanced SKN with a
  later index, or use firmware uptime as the epoch.
- This byte API deliberately does not impose/change the other worker's JSON or
  header format. Decode its explicit fields with bounded input before calling
  `parse`; no generated config or real account material belongs in this tree.
- `parse` clears its output on failure. Input buffers must not alias the output.
  Caller-owned provisioning, storage and callback/context must outlive `Core`.
- `Core::provisioned()` means admitted in-memory schedule; it is not a check that
  a physical file exists. `diagnostics()` contains only status enum, booleans,
  index and public x. Failed operations invalidate/zero `currentFrame` output;
  the HAL must stop advertising on any failure, not retain the old frame.
- Call `rotate` before consuming the frame and regularly while advertising.
  `currentFrame` is a pure snapshot, not a clock poll or expiry timer. No thread
  safety/reentrancy is provided; serialize lifecycle, storage and HAL calls.

## Key schedule and framing

`SK(i+1) = X963_SHA256(SK(i), sharedInfo="update", length=32)`.
Diversification uses X9.63 SHA-256 with `"diversify"`, length 72; the two 36-byte
integers reduce modulo `N-1`, then add one. The derived scalar is
`(u * master + v) mod N`, and the advertisement key is its big-endian P-224 public
x coordinate. A zero derived scalar or failed real curve operation is an error.
There is no custom elliptic-curve math or fabricated point validation.

`Frame.address` is display/controller order: first six public-x bytes, first byte
OR `0xc0`. **NimBLE's internal address array needs reversal in the HAL.**
`Frame.payload` is exactly 31 raw legacy AD bytes, beginning
`1e ff 4c 00 12 19 00`; public-x bytes 6..27 occupy bytes 7..28; byte 29 preserves
public-x byte 0's top two bits; byte 30 is zero. Do not append Flags or a name to
this full legacy packet. This pure encoder packs public bytes and does not
validate externally supplied x coordinates; the Core only feeds real derivation
results. HAL/controller packet acceptance remains separately qualified.

## Persistence and restart safety

Implement `Persistence::load(Checkpoint&)` returning `Load::Ok`, `Empty`, or
`Error`; implement `commit(const Checkpoint&)`. Checkpoint version is **1** and
fields are:

- `identity`: 28-byte public x at provisioned initial index (binding to identity).
- `publicKey`: 28-byte public x at persisted current index (seed corruption check).
- `seed`: **secret** current 32-byte primary seed.
- unsigned `initialIndex`, `epochSeconds`, `index`, `lastWallSeconds` (64-bit).

Persist fields explicitly in a versioned format; **do not dump the native C++
struct** (padding/endian/ABI are not a file format). Parent storage owns bounded
decoding, integrity/authentication, permissions, atomic replacement, durability,
and read-back verification. Return true only once the exact candidate is durable
and verified. The core does not claim host memory mocks prove flash power-loss
safety. Seed/public and index/wall consistency checks detect accidental corruption,
not malicious replacement/rollback of a complete older valid checkpoint. Protect
storage rollback externally if that threat matters.

The index is persisted **before** exposing its new frame. An uncertain failed
commit latches the core unprovisioned; no retry can emit the unconfirmed candidate.
Read the store again through `init` to recover. A stored index is treated as
consumed even if a crash occurred after commit but before BLE publication.

A restart in the same wall-clock slot returns `ReusedIndex`, exposes no frame,
and must wait for a strictly later slot. This deliberately accepts a temporary
tracking gap rather than silently reusing a persisted key after reboot. Do not
fall back to a static key. `allowFresh=true` is reserved for explicit **one-time
new provisioning** when the store is empty; normal boots must leave it false.
Never automatically erase/replace missing, unreadable, mismatched or corrupt
state and retry fresh. Deliberately deleting state and explicitly requesting
fresh provisioning cannot be distinguished by an isolated core; that policy
belongs to the owner-authorized provisioning transaction.

## Clock policy / bounds

Trusted absolute wall time is mandatory on init and every rotate. Target index is
`initialIndex + floor((unixSeconds - epochSeconds)/900)`. Backward wall-clock or
monotonic movement is an explicit error. Caller supplies an extended monotonic
millisecond counter; a raw wrapping `millis()` is insufficient. Against the
anchor at last publication, wall time may lag monotonic elapsed by at most two
seconds; a frozen/lost clock fails closed. A failure cannot revive the current
consumed key merely by restoring trust: wait for a later slot. No monotonic-only
fallback after time loss/reboot exists. Forward catch-up is bounded to 4096
ratchet steps per operation and all index additions are overflow-checked.

## Crypto backend requirements and verified boundary

Select **exactly one** build macro:

- Target: `FINDMY_CRYPTO_MBEDTLS`, existing mbedTLS 2.x/3.x headers/libraries with
  bignum, ECP, `MBEDTLS_ECP_DP_SECP224R1_ENABLED`, and SHA-256 enabled. Pass a real
  CSPRNG callback with signature `int(void*, unsigned char*, size_t)`; zero return
  means success. Used for scalar-multiply blinding. No ESP global RNG is assumed.
- Host: `FINDMY_CRYPTO_OPENSSL`, existing OpenSSL headers + `-lcrypto`; real
  `BN_*`/`EC_*`/SHA-256 implementation, not a mock adapter.
- Neither selected: encoder remains usable, but crypto/init explicitly report
  unavailable; no fake ECC validation or beacon readiness.

Executed here: g++ 15.2, OpenSSL 3.5.5 production adapter, and no-backend build.
The installed host has runtime mbedTLS libraries but **no development headers**;
no dependency was installed and mbedTLS/firmware target compilation was not run.
The mbedTLS branch's API fields were checked against upstream 3.6.4 headers,
not qualified by a target build. Target library configuration, compilation,
image/RAM budget, NVS power loss, RF byte order, native Items registration and
relay/decryption remain independent gates. No BLE activation or account writes
were performed.

## Tests / provenance

```sh
python3 test/findmy/core/run.py
```

The routine entrypoint compiles the actual two production translation units
against OpenSSL, runs the encoder, scalar/KDF, provisioning, rotation, restart,
clock, corruption and persistence-fault tests; compares four independent public
vectors at indices 0, 1, 2, 96; and compiles/runs a separate no-backend executable.
No SDK, installer, target build, sanitizer or in-tree artifact is involved.
Output defaults to `/root/.hermes/task-state/crosscub-findmy-implementation/core/`;
`FINDMY_TEST_OUT` can redirect it. RED/GREEN slice logs and the exact pinned
FindMy.py cross-check are retained there.

`test/findmy/core/vectors.json` is explicitly synthetic: deterministic master
bytes 1..28 and seed bytes 0..31. Fixtures were independently generated with
cryptography 50.0.1 X963KDF/SECP224R1, then verified by **executing the exact pinned
FindMy.py** `derive_ps_key`/`x963_kdf` source. Only public x values enter test logs.
See `NOTICE` for source pins, inspected paths and both MIT notices. No AGPL
firmware source was imported.
