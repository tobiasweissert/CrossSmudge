# Recoverable download storage — bounded design candidate

Status: frozen design candidate for the next implementation pass, NOT approved hardware activation. The existing API4 source and its atomic-backend contract remain unchanged and disabled on hardware. This document does not waive that contract.

## Executed prerequisite

`python3 rename_fault_probe.py` extracts the exact two-argument `FatFile::rename` method from the pinned local SdFat source and compiles it with instrumented dependency seams under ASan/UBSan. Six asserted cases passed (compile/run exit 0), including destination creation before failure, cluster aliasing before failed old-entry removal, and false after old-entry removal on final cache sync. The initial `sync()` return is ignored by the exact pinned method. `receipt.json` pins the full source, extracted method and generated compilation unit. These are control-flow/fault-seam results, NOT an execution of the full FAT implementation, block driver, actual SD card, or a power-loss qualification. The naive cleanup simulation deliberately demonstrates freeing a chain still referenced by the destination; it is not a measured physical corrupt-card incident.

## Decision and non-negotiable invariant

Do not implement `DownloadFiles::promote` as an SdFat rename wrapper. Do not retry ambiguous rename or remove either aliased path after failure. Do not flip the hardware capability flag.

A replacement internal store must preserve the previous validated generation without touching its data or directory entry while writing a replacement. It must avoid rename entirely. Do not promise a failed publication is indistinguishable from no publication: a failed storage acknowledgement can leave complete new bytes visible. This ambiguity must be represented explicitly, not hidden as a cancellation or a safe-to-retry error.

## Bounded replacement protocol

1. Use only the existing firmware-derived, app-confined root and two fixed data slots; never derive paths from URLs, Lua options or server headers. Preserve a validated previous generation and write only the inactive slot. No arbitrary handles, paths, origin changes, or new credential behavior.
2. Validate the entire candidate record through bounded incremental reads before selecting it: magic/version, nonzero generation, bounded length (2 MiB), exact file length, footer guards, and streaming payload checksum. The current footer-only validation is not sufficient for the replacement. Keep memory O(1024 bytes), no whole-archive allocation.
3. Distinguish absence, invalid record, and I/O uncertainty. A provably corrupt inactive record must not prevent access to a valid previous record; an unreadable slot may conceal a newer committed record, so return a storage uncertainty error rather than guessing. Equal-generation differing payloads and generation exhaustion fail closed.
4. Write the inactive slot with an initially invalid record. Complete body and checksum, sync and close, reopen and verify. Only then write a bounded final validity seal and sync/close/reopen for validation. Do not erase the previous valid slot during publication or abort. Reject cancellation before the seal; once publication may have occurred, cancellation cannot be reported as guaranteed no-change.
5. Use a typed internal result: `committed`, `not_committed`, or `commit_unknown`. A write/sync/close/readback error after the seal may have reached storage is `commit_unknown`, even if another read in the same session appears old. Stop automatic retry and further writes for that app until recovery examines both slots. Recovery may choose a fully validated newer generation, while retaining the previous record until a later successful transaction. Never erase data merely to make an error disappear.
6. The current API4 promise cannot silently absorb `commit_unknown`. First implement and test the internal protocol independently, then explicitly decide and review its versioned Lua response/recovery contract and client adoption. Keep API3 calls unchanged and hardware downloads disabled until that decision and backend review complete. This is an engineering contract decision, not a request for provider credentials or owner device testing before implementation.

## Next executable phase (no firmware build)

Add an independently tested portable `RecoverableDownloadStore` candidate beside—not in place of—the frozen API4 core, with a minimal fixed-slot file interface and an explicit three-state commit result. First assert the previous-generation invariant at every operation failure, ambiguous final seal, corruption, canceled-before-seal, canceled-after-seal, and interrupted/reopened-store boundary. Do not claim a fake file adapter qualifies SdFat. Subsequently bind this candidate to the actual pinned SdFat implementation with a host-backed block-device fixture and bounded write/cache-failure injection; retain the method-level probe as a regression, not a substitute for that fixture.

Required before activation: independent review of new source and explicit versioned contract, actual FAT fixture results, exact-source native parser/store/client regression, then one serialized target build with normal provenance checks. Required after delivery: owner's Sticky SD-removal, cancel, reopen, sleep, memory and existing-save tests. A FAT card/controller can corrupt unrelated metadata under physical failure; neither checksums nor fixed names establish universal power-loss safety.

## Scope and publication

No firmware/source mutation, commit, push, new upstream issue/PR, catalog update, installation, flashing, email, or paid service call is part of this prerequisite phase. The published combined release stays at c25248cbd04a7286fd049f2f1af68c48a91513d8. All new upstream publication remains gated on successful owner device testing and screenshots.
