# Retained grayscale HAL bridge regression

Run this directory's bounded regression entrypoint from any directory:

```sh
python3 test/crosspoint_migration/hal-retained-overlay/run.py --mode normal --evidence /path/to/evidence
python3 test/crosspoint_migration/hal-retained-overlay/run.py --mode sanitized --evidence /path/to/evidence
```

The parent migration test owner should register both invocations in the aggregate
entrypoint: this task owns only `lib/hal/HalDisplay.{h,cpp}` and this directory.
No aggregate test configuration is modified here.

## Tested boundary

* `contract.cpp` compiles the actual local `HalDisplay.h` and actual pinned SDK
  `EInkDisplay.h` / `FreeInkDisplay.h`, checks the alias and both exact member
  signatures, and compiles explicit/default-restore calls.
* `sdk-contract.cpp` independently compiles the SDK API before the HAL probe.
  SDK HEAD must equal `634a3f8c067a3714a6a7c622015138af096ab5d2`; all consumed
  display headers and the SDK facade implementation must match their Git blobs.
* The executable compiles the complete actual `HalDisplay.cpp` and
  `HalSpiBus.cpp`. Unrelated functions are discarded by linker section GC.
* Nine cases cover asymmetric arguments, restore true/false, success/failure,
  zero and uint16 boundary geometry passed through unchanged, the default false
  argument, recursive lock scope, and RAII unwind (synthetic exception).
* The real SDK class declaration is never replaced by a fabricated display
  header. Its constructor and retained-overlay method have **link doubles**:
  the latter records arguments, checks that the lock is held, and returns the
  selected result. `EpdBus` destruction is a link double too.
* Arduino/GPIO, SPISettings, BoardConfig hardware profile, logging and FreeRTOS
  semaphore calls are **hardware doubles**. The real SPI lock implementation
  runs against the recursive semaphore recorder; no physical scheduler or bus
  concurrency is claimed. The exception case tests host RAII only, not an
  exception-enabled firmware requirement.

This is real-SDK **declaration compilation**, not SDK facade/panel execution,
physical display qualification, or a Sticky target build. The SDK implementation
still owns geometry/controller/framebuffer/inversion checks, pending-async sync,
gray-pass cancellation, driver dispatch, and success-only shadow updates.
The HAL deliberately adds no validation or coordinate transformation.

The inherited `HalSpiBus::Lock` logs and continues when mutex initialization or
acquisition fails. Changing that policy is an out-of-scope seam; these tests cover
successful acquisition and lifetime only.

## Evidence / TDD

Before changing production, `--expect-red` passed the independent SDK declaration
probe and recorded the missing-member compiler failure on the real local HAL.
This flag is intended only for the unpatched source, not as a post-fix test.
Normal and sanitized modes record command/exit logs, input SHA-256 manifests,
executable SHA-256, and XML derived from each process outcome. Sanitizers are
ASan + UBSan, with leak detection, and run sequentially after normal mode.
Temporary compiler outputs live under `$TMPDIR`, are removed on exit, and are
not archived. The runner refuses to begin with less than 650 MiB scratch free.

Evidence for this repair lives at:
`/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/hal-retained-overlay/`.
Final frozen hashes are in `freeze.sha256` there (not inside this directory, so
there is no self-referential hash). No target build, SDK modification, install,
commit, index edit, or remote write is performed.
