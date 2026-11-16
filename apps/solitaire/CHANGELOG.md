# Changelog

## [1.1.0] - 2026-11-15

### Changed

- Use tested CrossPlay Lua gameplay on Sticky under the existing app ID; retain the exact original game and saves on all other devices and older firmware.

### Added

- Bounded, readback-verified two-slot saves, legacy save preservation, source-oracle and runtime tests.

### Fixed

- Direct card taps select then move; tapping the selected pile again deselects. Touch reset, overlapping runs and empty targets have runtime regressions.

- Preserve original modules/assets and missing-device fallback; isolate Sticky replacement dependencies.

- Correct stock touch argument order and keep draw/recycle/undo consistent.
