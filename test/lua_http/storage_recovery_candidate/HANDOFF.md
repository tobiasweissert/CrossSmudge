# Recoverable download storage: read-side candidate

Implemented and exercised this tick: full bounded sealed-record validation and deterministic two-slot recovery. This is a portable implementation candidate, not a new device feature or an approved replacement for frozen API4.

Run `python3 /root/github/.host-data/crosssmudge/http-storage-recovery/candidate/run.py`. It compiles/runs normal and ASan/UBSan modes with exceptions/RTTI disabled, rejects missing/duplicate test-group identities, and saves hashes and real exits in receipt.json. Both modes passed. red.log and red-recovery.log preserve expected assertion failures before each behavior was implemented. receipt-initial-runner-count-failure.json preserves an initial runner-only expected-total mistake: all executable assertions passed but the wrapper expected ten groups instead of the nine named groups. The final runner reconciles exact names.

The candidate reads only numeric slots 0/1 and validates an explicit little-endian format: 24-byte header (`RDS1`, version 1, uint64 generation, uint32 length, uint32 FNV1a checksum), bounded payload, and matching 24-byte seal (`SEAL`, version 1, generation, length, checksum). Maximum payload is 2 MiB. Read requests are at most 1024 bytes and memory use is constant, independent of payload length. No production format or API has been silently changed. FNV1a is accidental-corruption detection, NOT cryptographic authentication. Equal-generation records are compared byte-for-byte; a generated real FNV1a collision is covered.

Recovery distinguishes absent/corrupt slots from uncertain I/O. Corrupt newer records fall back to a validated older generation; unreadable slots fail closed instead of guessing. Identical equal generations select slot 0; differing equal generations report conflict. Reading UINT64_MAX is allowed; any future writer must reject generation increment exhaustion. The Files adapter is synthetic in-memory storage only. The eventual caller must hold exclusive app-store ownership throughout validation/selection/publication and use an adapter with stable exact reads; this candidate does not solve concurrent mutation, malicious storage, FAT/controller power-loss behavior or unrelated card corruption.

## Next bounded phase

Implement write/seal/abort in this isolated candidate using strict behavior-first tests, preserving the working read side. Define explicit committed/not_committed/commit_unknown results, reject writes on uncertainty/conflict/generation exhaustion, and retain the previous validated slot across every injected operation failure. Verify unsealed candidate content before writing a seal; errors after seal admission must remain commit_unknown even if an immediate read appears old. Add cancel-before/after-seal and reopen regressions and preserve RED receipts. Do not enable firmware downloads, mutate the frozen API4 core, publish, or compile firmware as part of this slice.

Then exercise the actual pinned SdFat implementation with a host-backed block-device fixture. Independent source review, explicit versioned Lua contract/client adoption, target compilation and physical SD acceptance remain separate pending gates.

## Ownership and integration

No workers launched. No source checkout files, manifests, catalogs, frozen core, firmware artifacts or remote state were changed by this slice. Existing combined working tree was already dirty at observation time (catalog plus app manifests); its HEAD was unchanged c25248c. Preserve those externally owned edits, do not stage or publish them, and resolve current ownership before resuming integration.
