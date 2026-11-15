# Candidate staged-readback phase

Scope: isolated candidate only. No production checkout changes, firmware rebuild, Lua API activation, device or upstream publication.

Added `PreSealCheck.h`: bounded read-only verification of exact staged extent, still-zero header/footer and body checksum supplied by the accepted-input accumulator. Uncertain size or read results fail closed; no rename/delete/seal operation is performed. The checksum is corruption detection, not authentication. Caller must retain exclusive app storage ownership; later commit must check cancellation immediately before sealing. This check alone does not authorize publication or prove durability.

Expanded body-stage regressions cover each source-read failure combined with failing close, and cancellation combined with failing close. Old slot remains unchanged and incomplete candidate never validates as published. Cancellation result does not claim that failed close released an underlying handle; the eventual real backend must resolve handle ownership explicitly.

Executed `python3 run_all.py`: recovery, admission, body and preseal suites in normal and ASan/UBSan modes all compiled and exited zero. Full source fingerprints, exact suite groups and exits: `all-receipt.json`. Missing-header RED retained in `preseal-red.log`. Independent fixture checksum calculation found and corrected the test's provisional literal before first successful compilation. The prior receipt is retained as `all-receipt-before-preseal-20261115T1707.json`.

Next bounded phase: design/implement versioned sealing with committed/not-committed/indeterminate outcomes and failure injection for mutation-before-error, sync, close and readback. Retain old valid slot, never blindly remove uncertain files. Then implement/qualify actual pinned SdFat backend, obtain independent review and integrate the versioned Lua contract. Input-accumulated expected checksum must be wired from the real staging path, not trusted from an arbitrary caller. Numeric slot confinement and exclusive ownership remain backend responsibilities. Physical power-loss/SD behavior is not established by these synthetic adapters.
