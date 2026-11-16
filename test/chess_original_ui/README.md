Run `python3 test/chess_original_ui/run.py --work-dir /tmp/chess-ui`.
The routine builds the shared production extractor, opens real production
libraries/Sticky identity and runs actual input callbacks in the vendored
int32/float32 VM under2MiB. File/time are declared deterministic fixtures;
all glyph masks, Celtic pieces and drawing primitives are shipped production
bytes/code. It fails on clipped glyphs/sprites or a required native pixel mismatch.
`--app-dir` tests an extracted SD package.

Reference PNGs and their hashes are in `reference/`. `build_native.py --output DIR`
uses the unchanged frozen native renderer prefix and original d3a49ce8 Chess
builders/Activity painters/SDK/Epd fonts. Only the dispatcher maps actual target
FEN, selection, SAN history and UI fields into original structs. The native
engine/core is not replaced by a guessed picture. Reproduce Jersey cuts with
`repack.py`; approved existing piece assets are unchanged.

21required native gates pass:17full-frame and4scoped settings comparisons.
Fresh/saved menus and frontend settings match both orientations. Eleven portrait
board states match full-frame, including select/hints, AI thinking, recent moves,
checkmate/stalemate, fifty-move/repetition, actual underpromotion reopen and5-digit move
number. Eleven landscape boards retain the source's real24px piece cut because the
original unadapted painter puts48px pieces in28px cells; differences are reported,
not counted as exact. Native landscape also leaves no room for its recent move
list; the full existing counter and recent captions are visible in portrait.

The existing target FIND HINT and SAVE AND EXIT remain2additional board settings
rows. Portrait masks exclude exactly those2rows per board/restart state. Landscape
masks cover the changed scrollbar thumb, and in the4-to6-row P&P transition only
the changed6px border/track and exact5px-shifted RHS glyph unions. Every rectangle,
compared-pixel count and raw full-frame difference is in results.json. No model
limit is hidden behind a full-frame equality claim.

Nearby keeps the original door/icon but reports the missing host backend without
changing the game. The existing saved-mode schema has Computer White/Black and
P&P, with no F2F field; no migration or invented network game is added. Four-choice
promotion is retained from the target; the original Activity auto-promotes Queen.
Move Hints remains session-only as before. Opponent/colour preferences apply when
the labelled settings footer confirms the new game, preserving the journal schema.
The source contains no Chess HowTo screen. No help/promotion reference is invented.

The root-owned14-case replacement routine retains all oracle/legal-history,
AI/search/cancel/undo, promotion, reload and memory assertions. Its runtime/lean
fixtures now enter the native front door, open settings through the gear, follow
rotated P&P coordinates and scroll to drawn controls. Non-Sticky legacy files and
approved sprites remain byte-identical;12legacy callback traces and12separate
75KiB observations are compared by the existing preservation entrypoint. Such
adapter traces do not qualify host-font pixels or C3 gameplay support; Sticky
under75KiB still refuses before reading saves.

Hardware check on Sticky, with existing journals preserved: launch upright and
sideways; fresh/saved menu and Nearby refusal; settings from both entry points,
scroll all rows and confirm pending colour/opponent changes; play both colours,
P&P rotation, move-hint dots on/off, AI cancel/resume, take-back, all4promotions,
result/play-again,5-digit move captions and reopen. Simulate a failed SD write and
retry without deleting either slot or legacy backup. Host verification does not
qualify physical refresh, touch orientation or SD timing. No flash is part of it.
