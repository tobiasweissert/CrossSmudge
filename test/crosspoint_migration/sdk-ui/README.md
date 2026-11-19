# SDK/HAL/UI bridge bounded regression entrypoint

```sh
python3 test/crosspoint_migration/sdk-ui/run.py --mode normal --evidence "$TMPDIR/sdk-ui-checks"
python3 test/crosspoint_migration/sdk-ui/run.py --mode sanitized --evidence "$TMPDIR/sdk-ui-checks"
```

Requires Python, g++ with C++20/ASan/UBSan, TMPDIR, and read-only immutable Git objects. No install, SDK initialization, firmware build, simulator, network or device access occurs. SDK overlay imports use an isolated empty project, never the real dependency checkout. Each child has a 40-second deadline and durable independent output/exit receipt.

- Normal: ten explicit entries. Sanitized: eight native entries (structural build/overlay checks are normal-only).
- `test_clock.py`: complete production HalClock TU over RTC/network/ESP-IDF doubles, UTC versus DST/legacy offset/date/cache/system-sync interfaces.
- `test_storage.py`: extracted production bounded read over IO doubles; cap/heap/short-read/checked-close failures leave empty output.
- `test_touch_snapshot.py`: extracted actual snapshot builder, long-press opt-in, held input and off-target release/swipe cancellation.
- `test_overlay.py`: exact pinned SDK transformations, installed idempotence and unknown-input rejection without writes.
- `test_secure_client.py`: complete overlaid SDK SecureClient TU over explicit TCP/wolfSSL doubles; peer/root/hostname/min-version configuration, trust revocation, numeric TCP pinning without DNS, retry, cancellation and cleanup. **Not cryptographic certificate validation.**
- `test_ui_host.py`: real UiAppHost header and implementation bodies, dependent SDK doubles; initial/reset/close versus repaint routing, aliases and held-input opt-in.
- `test_input.py`: actual input header and additive production methods; remote/contact collision refusal, one event, orientation and held-button suppression. Legacy mapping/physical dispatch is doubled.
- `test_themes.py`: real theme headers/overrides/designated initialization and extracted cover clipping/header-band adapter; renderer double, **not real-font pixels**.
- `test_activity_results.py`: actual header bytes with model type doubles; owner aggregate prefixes/decimal precision/variants plus new progress/clipping fields.
- `test_build_contract.py`: structural profile/dependency/patch/slot/version/icon/error-interface invariants and Python syntax.

By default immutable SDK objects come from `/root/github/crosssmudge/.git/modules/freeink-sdk`, and owner/upstream refs from `/root/github/crosssmudge`. Override with `CROSSCUB_SDK_GIT_DIR` (actual Git directory) and `CROSSCUB_CANONICAL_REPO`. Required SDK SHA: `634a3f8c067a3714a6a7c622015138af096ab5d2`; firmware refs: `refs/audit/crosspoint-rebase/{owner,upstream}`. A CI consumer must supply those objects explicitly; do not silently fetch moving heads.

This suite qualifies a partial primitive layer only. ActivityManager, RenderLock, OptionPopup and main remain unresolved in the writer handoff; no successful whole-firmware compile, BLE/AirDrop/Sync lifecycle, keyboard long-draft acceptance, publication or physical acceptance is implied.
