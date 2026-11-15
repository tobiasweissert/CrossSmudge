# Sea Salt local adaptation

All 58 original card identities/colours, collectors, mermaids, multipliers, draw/keep/discard, crab pile search, fish draws, boat extra turns, swimmer/shark steals, Stop/Last Chance, protected revealed hands, deck-exhaustion zero scores, 40-point matches and four-mermaid immediate wins.

## Differences

New own-hand/public-card heuristic, not SeaSaltBrain. Solo only; Nearby is not implemented. The PSRAM presentation now uses original CrossPlay card/colour art and Jersey masks. Duo selection is two successive card choices to bound memory. Card inspector protects hidden opponent cards. Original labels retained; colour is written in text.

## Controls and saves

Up/down (or page buttons) scroll the action list; Confirm selects; taps select visible rows. Back saves/exits. Inspect screens return on selecting a row; use navigation buttons to scroll. New-game/case selection is explicit and cancellable. App-relative checksummed two-slot journals validate decoded state, read back writes and fall back from torn/invalid latest slots. Autosaves are debounced to five seconds on actions; Back/on_exit always attempts saving. Sudden power loss can lose recent actions. No global firmware state is written. English only.

## Verification boundary

Stock vendored Lua int32/float32, serial native adapter runs at 76,800 and 2,097,152 allocator bytes, both orientations, complete games/cases, journal failures and independent rule review. Source-oracle full-game traces use 2 MiB to include instrumentation; separate smaller full-game regressions run at 75 KiB. This does not measure C/HAL allocations or prove physical-device headroom. See repository docs/crossplay-ports.md and external five-runtime-results.json/five-oracle-results.json for exact evidence.

On Sticky v1.6.5.3, install this new folder without overwriting saves. Test buttons/taps in portrait/landscape, every inspection/setup menu, a full match/case, exit/reopen and sleep/wake. Inspect Application Error/OOM logs, internal/PSRAM free and largest blocks. No EPUB cache reset. No physical hardware or visual-fidelity test is claimed.

## Original-screen presentation

On Lua heaps of at least 512 KiB, the app now has the original card-and-pile board vocabulary: four-across card pages, public hand/table tabs, colour census marks, selected pairs, KEEP/PLACE/DIG choices, the score/call rail, STOP/Last Chance cards and the six illustrated help pages. Original Noto/Jersey/Lucide mask bytes are sourced by `test/seasalt_graphics/assets.py`; `art/sources.json` records the source-header hashes. The 75 KiB dispatcher loads the byte-preserved compact adapter.

Match played/won records are now kept in separate checked `ui-record-a.dat` / `ui-record-b.dat` slots. They start from the completed match available at installation, if any; earlier unrecorded match history cannot be recovered. A match is counted once across reopening. This does not change the existing gameplay snapshot format.

Still different from CrossPlay: the original Nearby mode and SeaSaltBrain are not implemented; the existing solo heuristic remains. The portrait board and help use reference geometry; landscape uses an adapted arrangement. The front door omits the unsupported Nearby action. Glyph masks are repacked losslessly, but advances are rounded to whole pixels and original font kerning is not reproduced. This is not a claim of pixel-for-pixel parity on physical e-ink.

Run `TMPDIR=/tmp python3 test/seasalt_graphics/run.py`. It checks full-match rendering states, exact action indices, hidden-opponent filtering, rendered two-card pair selection, closed-input behavior, journal resume and record recovery in both orientations, plus the original 75 KiB capacity/lifecycle cases. Native bitmap captures are written under `/tmp/seasalt-ui-captures`; they are not hardware evidence.
