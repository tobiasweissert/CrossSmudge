# Native AirDrop receive integration

Sticky-only receiver built on the pinned licensed ESP32Drop AWDL foundations.
The implementation is integrated in source; final normal Sticky firmware build
and physical interoperability remain explicit qualification gates. See the task
evidence in `/root/github/.host-data/crosssmudge/airdrop`.

- `core/`: bounded binary-plist offers, strict incremental HTTP framing and
  fresh one-use consent tied to peer, interface and session generation.
- `session/`: actual TLS-session protocol orchestration. Every transfer waits
  for Accept or Decline. A file Upload is authorized on the same live accepted
  TLS connection; reconnects are rejected rather than inheriting consent.
- `stream/`: fixed-workspace DVZip stored/zlib to odc CPIO decoder, one accepted
  `.epub` or `.txt`, at most **30,000,000 payload bytes**. No whole-file PSRAM.
- `storage/`: bounded streaming to exclusive SD staging, checked validation and
  publication, immutable recovery journals. Uncertain FAT outcomes retain
  evidence and prevent further native publication pending explicit SD repair.
- `port/`: owning-task TLS lifecycle, interface-isolated listener, bounded
  discovery callback, cooperative AWDL shutdown and Arduino radio restoration.
  A stalled owner remains visible; there is **no automatic restart fallback**.
- `browser/`: immutable pending native URL with attempt-bound, one-use launch
  acknowledgement. The installed LuaBrowser receives it through its normal
  opening path after native ownership stops. Failure retains explicit retry
  or cancel; scheduling a child does not acknowledge delivery.

App policy and File Transfer UI live in
`src/activities/network/AirDropReceiveActivity.*`. Existing outgoing Browser
HTTPS verification remains enabled. Only the inbound Everyone listener uses
its foundation self-signed identity; this makes no verified-Apple-identity claim.

`test/airdrop/` exercises production components, actual pinned SdFat fault
behavior, production coordinator/UI methods and the adapted AWDL lifecycle.
ASan/UBSan are enabled in sanitized runs; LeakSanitizer is unavailable under
sandbox tracing. Fakes at socket, crypto, scheduling and radio boundaries are
identified in the evidence and do not constitute device proof.

Real iOS/macOS discovery (including modern TXT flag acceptance), same-session
Upload behavior, maximum-size SD hashes, memory/stack headroom and repeated
seamless mode switching require physical qualification. Do not claim these from
host tests or the original foundation link-only probe.
