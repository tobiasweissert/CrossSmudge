# Crosscub repository development guide

Crosscub is Sticky-only. [SCOPE.md](SCOPE.md) is the product/readiness ledger;
[architecture](docs/development/architecture.md) describes the direct CrossPoint
foundation and compatibility seams. This candidate is experimental, not a release.

Read the narrow entrypoint and direct collaborators first. `src/main.cpp` and
ActivityManager own lifecycle; `lib/Epub` owns layout; `lib/GfxRenderer` and
SdCardFontSystem own rendering/font lifetime; `src/network` owns native transfers;
`src/activities` owns owner UI; `apps/` and Lua runtime remain distinct from plugins.
Use firmware HAL rather than GPIO/SDK singletons in application policy. The pinned
FreeInk SDK supplies shared hardware/UI/network/font primitives; do not edit vendor
board catalogs to make branding searches empty. `UiAppHost` is non-template.

Preserve owner enum IDs, settings/recovery roots, saves and licenses. Never replace
owner raw enums with display indexes or silently narrow value16 settings. Never erase
durable progress/clippings/stats to invalidate a derived reader/library/font cache.
Renderer tracking and following-codepoint advances are separate APIs. Publish touch
geometry with paint; require fresh post-modal input and radio/storage ownership.

Build only `pio run -e sticky` with the pinned outer/nested Core 6.1.19, platform
55.03.311 and SDK 634a3f8c067a3714a6a7c622015138af096ab5d2. Run the applicable
portable suites and `python3 test/crosspoint_migration/docs-ci/test_policy.py`;
report measured exits and exact source hashes. Portable checks do not establish
SDK compilation, physical FAT, cryptographic TLS or real-font/display behavior.

Owner controls stable release, Store and branding. PR/manual CI is artifact-only;
no publication or main/tag mutation is implied by source work. Keep Crosscub,
CrossSmudge/CrossInk/CrossPoint/FreeInk and component credits honest.
