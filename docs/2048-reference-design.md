# 2048 1.1.0 — local Sticky design update

## Changed

- Sticky uses a spacious title/live-score header, rounded tonal tiles with bundled large Lato glyphs, and a Best/New game footer. Portrait and landscape layouts fit the available bounds. Other devices retain the original presentation and controls.

## Added

- Best score survives New game and app restarts. With no previous history, Best starts at the current saved score; no historical score is invented. Two validated Best keys provide recovery if one copy is damaged.

## Fixed

- Sticky rejects malformed board/score records before rendering. Existing valid board, score, and game-over saves remain compatible.

This app-only candidate is not published. Its original swipes, button mapping, merge rules, app ID, name, and icon remain unchanged. `legacy_main.lua` is byte-identical to the original app. The new entry point adds about 1 KB of measured peak Lua overhead on the legacy path without loading font assets.

Run `/usr/bin/python3 test/2048_design/run.py <evidence-directory>` from the repo root (Linux, C compiler, Pillow). The runner compiles only vendored Lua 5.4.7 with the repository's int32/float32 configuration and opens the same five standard libraries as local production apps. It does not build firmware. Production `on_draw()` feeds a native logical-framebuffer fixture implementing the shipped sprite polarity/transparency and dither formulas. Captures use the exact bundled glyph bitmaps; these are host fixtures, not device screenshots. Lato Heavy/Regular is a close sans-serif substitute, not the exact reference font. The display's 25%/50% dither tones differ from the smooth grays in the reference image.

For hardware acceptance, install the test ZIP in the `2048` app directory on a Sticky whose firmware reports `sticky` through `get_device()`. Keep its save directory. Verify all four swipes, cancel reset with Back and the Cancel button, confirm New game, and exit/reopen to check the board and Best. Rotate and check every tile and both modal buttons. Inspect e-ink dither and text legibility. No EPUB cache reset is needed. On X3/X4, check the original screen and controls. No physical-device acceptance is claimed by native tests.

Storage caveat: the current runtime's `smudge.save` returns no success status and writes each game key independently. Best uses readback and a backup; it cannot guarantee persistence through total SD failure or atomic board/score updates across power loss. Original game arithmetic uses signed 32-bit Lua integers; this update does not change its overflow behavior.

The parent should merge only the owned file allowlist and the single catalog record by ID, after the other integration writer releases its checkout. Never replace the integration catalog wholesale with this older canonical catalog. The 2048-only changelog sections above are the scoped changelog delta for the parent's combined changelog.
