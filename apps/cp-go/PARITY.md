# Go local adaptation

Captures before suicide, simple ko and source eight-position situational repetition window, source 5*N*N move limit, 7.5 komi (0.5 for handicap), 2-5 handicap stones on 9x9, two passes, group dead marks, area scoring and two-sided acceptance; disagreement resumes play. 9/13/19 shared-device boards.

## Differences

NOT full-history superko: matches the source bounded window and move-limit house rule. Optional 9x9 one-ply capture/liberty opponent, not Michi/search/difficulty parity. Human adjudicates dead groups and both score acceptances, including solo. No Nearby, automatic life/death, resign button or original statistics. The 75 KiB UI uses coordinate rows; handicap UI is 9x9 only although core supports all three sizes.

## Controls and saves

Up/down (or page buttons) scroll the action list; Confirm selects; taps select visible rows. Back saves/exits. Inspect screens return on selecting a row; use navigation buttons to scroll. New-game/case selection is explicit and cancellable. App-relative checksummed two-slot journals validate decoded state, read back writes and fall back from torn/invalid latest slots. Autosaves are debounced to five seconds on actions; Back/on_exit always attempts saving. Sudden power loss can lose recent actions. No global firmware state is written. English only.

## Verification boundary

Stock vendored Lua int32/float32, serial native adapter runs at 76,800 and 2,097,152 allocator bytes, both orientations, complete games/cases, journal failures and independent rule review. Source-oracle full-game traces use 2 MiB to include instrumentation; separate smaller full-game regressions run at 75 KiB. This does not measure C/HAL allocations or prove physical-device headroom. See repository docs/crossplay-ports.md and external five-runtime-results.json/five-oracle-results.json for exact evidence.

On Sticky v1.6.5.3, install this new folder without overwriting saves. Test buttons/taps in portrait/landscape, every inspection/setup menu, a full match/case, exit/reopen and sleep/wake. Inspect Application Error/OOM logs, internal/PSRAM free and largest blocks. No EPUB cache reset. No physical hardware or visual-fidelity test is claimed.


## Original graphical presentation

With at least 512 KiB Lua budget, the app now uses the original Jersey masks, fixed-size goban, per-size grid pitch/star points, seat capture bands, stone/last-move/aim marks, and original counting/result territory shapes. A first board tap aims and a second tap on that point commits; counting taps toggle dead groups. Pass, both score acceptances, Play On, result Play Again/Done, 9/13/19 setup and saves use the existing rules and journal. Landscape fits the board and controls to the shorter screen. Menu actions and opponent strength remain the local adaptation, not original level/difficulty, lifetime stats or tutorial parity.

`python3 test/board_visual_ports/run.py` verifies both budgets/orientations, real point hit geometry at all three sizes, two-tap aiming, counting/result rendering and journal equality after touch. Draw-call captures are native test artifacts, not e-ink screenshots. On Sticky test both taps, Pass twice, dead-group toggling, both acceptances/Play On, all board sizes and save/reopen in both orientations. No cache reset is needed.


The larger-heap app opens on the original three-door layout and a miniature of the current 9/13/19 game. Setup choices are behind Settings; unsupported Nearby is replaced by Save and Exit. The record band reports current saved-game size/move state, without invented played/won totals. The supplied Go source contains no illustrated HowTo pages to port.
