> Historical provenance/checkpoint record, not Crosscub build or installation guidance. See [Crosscub](../README.md).

# Continue Sticky development on the Mac

## Start here

```sh
git clone --recurse-submodules --branch integration/sticky-test-all https://github.com/tobiasweissert/CrossSmudge.git
cd CrossSmudge
```

This branch is the combined **work-in-progress source checkpoint**, not a newly verified firmware release. Server implementation has been stopped at the owner's request. The CrossSmudge 10-minute completion coordinator (`46ebb6199904`) is paused; do not resume it automatically.

## What is included

- App Store touch Wi-Fi controls, remote icons and screen-sized layout.
- Eighteen local CrossPlay adaptations and Hacker News/Trivia API3 previews.
- Sticky-specific Chess/Sudoku/Solitaire/Wordle replacements; original implementations remain the fallback on other boards.
- Desk Stand/Hourglass/Water Tracker touch repairs.
- The real board-identity repair: use value-checked `FREEINK_DEVICE_*` flags. `defined(CROSSINK_APP_DEVICE_X4CLASSIC)` was true even when its value was zero, making Sticky report `x4` and load legacy games.
- Native UTF-8 registration for valid network apps and default-enabled networking, with an optional global opt-out. Certificate/clock validation, declared HTTPS domains, public DNS/SSRF checks, bounds, cancellation and app-file confinement remain intact.
- Twenty approved, correctly oriented icons. Use `scripts/convert_icon.py`: upright artwork is rotated 90 degrees counterclockwise before packing. Valid store-icon caches are keyed by app ID and can otherwise retain older icons.
- Display names without the `CP ` prefix; internal `cp-*` IDs and save paths remain unchanged.
- 2048 1.1.0 reference-style Sticky UI, real score/Best, save compatibility and original non-Sticky implementation.
- **New Minesweeper graphical UI and opt-in long-press source/tests. Its firmware build was interrupted for this Mac handoff.** Rebuild and verify before claiming hold-to-flag works on hardware. Preserve the released 2048 catalog entry when continuing Minesweeper.

## Last verified firmware, versus current source

Last verified and delivered binary: `1.6.5.3-test.8108543.27d94c0f`.

- BIN SHA256: `ac583dab67fd7574f065433e2972f56fb0254a0b02962cfb55de37e75ca8a1d8`
- Matching ELF SHA256: `d341306736ce7d6a033302e422bf27e6eb0d9c12f14af3d7af95b8e4849aa957`

It includes the board-identity fix and HN UTF-8/default-network changes. It does **not** include the newly added long-press host dispatcher. App-only updates such as icons, names and 2048 are separate SD packages.

Archived BIN/ELF files, full logs and private owner screenshots remain on the Linux host under `/root/github/.host-data/crosssmudge/`; they are intentionally not added to Git. Do not regenerate an ELF and use it to decode a different historical binary.

## Other saved branches in this fork

```sh
git switch --track origin/feat/lua-http-streaming-followup
```

- `feat/lua-http-streaming-followup`: experimental API4 download/read-at core, staged Connections package, and API5/recoverable-storage prototype work. **Hardware activation remains disabled.** The pinned SdFat rename can fail after mutating directory entries; naive cleanup can free aliased clusters. See `docs/lua-https-v4.md`, `docs/lua-network-followup-state.md` and `test/lua_http/storage_recovery_candidate/`. Prototype runners contain historical Linux dependency paths that must be adapted explicitly on the Mac, without patching SdFat production behavior to make tests pass.
- `fix/api3-draft-alignment`: separately prepared alignment of the draft API3 branch with device reporting, UTF-8/default-network policy and startup tests; not a substitute for the combined branch.
- `feat/sticky-2048-reference-design`: independently saved 2048 changes on the older local-port base. The combined branch already contains the actual app update.
- `feat/crossplay-game-replacements`: four independently developed replacement games and their source-oracle/preservation tests.
- `fix/sticky-app-touch`: three narrow app touch fixes/tests.
- `feat/lua-network-apps`: original stacked HN/Trivia preview branch.
- `archive/crossplay-browser-chat-wip`: **older paused CrossPlay firmware**, with SD-executable Browser/Chat work. This is not the active CrossSmudge Lua firmware. Both projects share the CrossPoint fork network, so the archive is preserved in this same fork rather than replacing the primary branch.

Do not merge the experimental streaming/recovery branch into the working API3 firmware merely to consolidate files. Do not replace the combined catalog with one of the older independent branches' complete snapshots; reconcile individual app records by ID.

## Build and test notes

The proven Linux stack used PlatformIO **6.1.19**, pioarduino/espressif32 **55.03.37**, GCC **14.2.0**, and SDK pin `b784446302c076b4b5a622627867f0c80905cc50`. Start from the repository's pinned configuration, not a generic latest-platform upgrade. The Mac dependency/toolchain installation has **not** been exercised by this handoff.

After configuring PlatformIO on the Mac, the intended application profile is:

```sh
pio run -e sticky-owner-test
```

Do not copy Linux `.pio` build/cache directories to the Mac: generated files contain absolute paths and platform-specific tools. Native Lua tests require a host C/C++ compiler, and render tests use Pillow. Pre-generated font assets ship with their licenses; font regeneration may require the documented font packages.

Useful focused test locations:

- `test/lua_device_identity/`: compile actual board reporting and drive real Lua Solitaire tap dispatch, not a hardcoded `get_device = sticky` fixture.
- `test/lua_startup/`: exact runtime-library initialization, native UTF-8, startup/default-network policy and HN fixture handling.
- `test/minesweeper_graphics/`: graphical rendering, core behavior, opt-in long-press, suppression of the release tap and non-opting-app compatibility.
- `test/2048_design/`: actual Lua rendering, score/Best/save/reset controls, deterministic differential moves, legacy traces and memory.
- `test/replacement_*`, `test/sticky_app_touch`, `test/lua_http` and `test/crossplay_ports` retain their respective scoped tests.

## Acceptance and publication gates

- Never uninstall or wipe SD apps to recover from an update; preserve `/.smudge/saved_data`, app-local data and old schemas.
- Check actual Sticky identity -> app entry -> touch behavior. Earlier app fixtures missed the firmware identity bug by supplying `sticky` directly.
- Check actual production Lua libraries. Earlier network fixtures hid the absent UTF-8 library by opening more libraries than the firmware.
- For Minesweeper, inspect the CrossPlay reference and real rendered output, then test short-tap dig, hold flag/unflag exactly once, no dig on release, movement cancellation, both orientations, saves and game rules.
- Clock trust currently needs independently verified UTC after reboot/deep sleep. This is separate from app network permissions; do not disable certificate checks as a convenience fix.
- Native tests and a build do not establish physical e-ink/touch/SD/TLS acceptance. Keep unfinished results labeled honestly.
- Existing upstream PRs remain as they were. Do not create/close/merge/retarget upstream PRs as part of this checkpoint. Owner device testing/screenshots still govern new submissions. Older per-app PR payloads require refreshing for the current icons, names, versions and host dependencies.
- Connections, Study, xkcd and Instapaper are not delivered as completed live apps; provider, credential, content-license and runtime/storage blockers remain explicit. No all-33-app parity claim.

The server task directories and worktrees are retained for recovery; no cleanup or branch deletion was performed for this handoff.
