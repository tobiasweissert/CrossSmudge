# Write admission slice — not a storage implementation

Implemented `WriteAdmission.h` beside the frozen read-side recovery candidate. The read-side bytes and firmware/API4 sources were not changed. `admission-red.log` records the missing-feature compile failure before implementation.

`python3 run_all.py` is the combined routine entrypoint: both recovery and admission run in normal and ASan/UBSan modes with exceptions/RTTI disabled. `all-receipt.json` records exact source hashes, measured compiler/run exits and terminal group identities. All four invocations passed: nine recovery and six admission groups per mode.

Admission selects only the inactive numeric slot, starts an empty store at generation one, rejects generation overflow, uncertainty/conflict, cancellation, oversized payload and malformed selected records. It performs no writes. The returned plan is not a lease: the future integration must retain exclusive per-app store ownership from recovery through final publication; callers must not supply stale recovery snapshots.

Next executable slice: add a separate writable fixed-slot interface and transaction implementation using tests that first fail for interrupted body writes, pre-seal sync/close/readback and cancellation. Then implement final sealing with committed/not_committed/commit_unknown outcomes; preserve the previous slot throughout, retain uncertainty until both slots are recovered, and fault-inject every operation. Do not enable hardware downloads, weaken API4, or treat this admission slice as a finished write/seal/abort protocol. Actual pinned SdFat/block-device and independent review remain pending.

Integration checkout is owned by the live Solitaire device-regression worker. No firmware compiler, publication, commit, push, upstream issue/PR, or source deletion was performed by this slice.
