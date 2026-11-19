# Toy Battle local adaptation

All nine source playable map topologies, HQ/medal/stuck endings, 24-troop shuffles/four set aside, rack limit, joker covers, reachability, buried stacks, all eight troop kinds, Capn chains, all seven base effects/restrictions, frozen troop expiry and atomic invalid-move rejection. Hidden Proving Ground is included for rule testing but excluded from map picker.

## Differences

New public-board/rack-size heuristic, not ToyBattleBrain; it does not plan Capn chains or special-base effects. Human can configure all effects and chains with the placement builder (0 declines, 1 uses; zero target means none). Solo only, no Nearby. At the 75 KiB limit the map inspector gives numbered adjacency/regions/effects.

## Controls and saves

Up/down (or page buttons) scroll the action list; Confirm selects; taps select visible rows. Back saves/exits. Inspect screens return on selecting a row; use navigation buttons to scroll. New-game/case selection is explicit and cancellable. App-relative checksummed two-slot journals validate decoded state, read back writes and fall back from torn/invalid latest slots. Autosaves are debounced to five seconds on actions; Back/on_exit always attempts saving. Sudden power loss can lose recent actions. No global firmware state is written. English only.

## Verification boundary

Stock vendored Lua int32/float32, serial native adapter runs at 76,800 and 2,097,152 allocator bytes, both orientations, complete games/cases, journal failures and independent rule review. Source-oracle full-game traces use 2 MiB to include instrumentation; separate smaller full-game regressions run at 75 KiB. This does not measure C/HAL allocations or prove physical-device headroom. See repository docs/crossplay-ports.md and external five-runtime-results.json/five-oracle-results.json for exact evidence.

On Sticky v1.6.5.3, install this new folder without overwriting saves. Test buttons/taps in portrait/landscape, every inspection/setup menu, a full match/case, exit/reopen and sleep/wake. Inspect Application Error/OOM logs, internal/PSRAM free and largest blocks. No EPUB cache reset. No physical hardware or visual-fidelity test is claimed.


## Original graphical presentation

With at least 512 KiB Lua budget, the board uses original Jersey masks and the exact source coordinates for all nine playable maps (and the test-only Proving Ground), paths, region medal anchors, slab/special/gate shapes, base badges, HQ labels, stack-depth marks, inverted friendly troops and duplicate-card rack. Source troop-effect glyphs and reserve/discard counters are drawn with primitives; original 480x800 geometry is retained, with smaller slabs and a side rack in landscape. Selecting a card then a base opens the existing explicit plan editor; it does not silently commit effects. Draw 2 and AI turns use the unchanged core.

The original progressive draft/effect-question workflow, result summary, lifetime statistics and Nearby remain unported; the plan editor and advanced inspector still show action lists. On the constrained path, collection before drawing/actions/saves clears temporary prior-frame tables before parsing effect/journal modules; no rules or save formats changed. Passing 75 KiB checks does not imply physical HAL headroom.

`python3 test/board_visual_ports/run.py` checks both budgets/orientations, every terrain's visible slot centers, card/base draft through a committed placement, and exact core/journal equality. Captures include multiple maps in both orientations. They are native draw-call captures, not hardware screenshots. On Sticky check Draw 2, duplicate rack cards, legal-candidate brackets, each special effect/Capn plan, complete a game, then save/reopen in both orientations. No cache reset is needed.


The app now opens on the original terrain miniature and bottom doors (Play/Continue, New Game, How to Play, Settings). The preview carries the actual saved position and terrain/goal, not fabricated lifetime records. Settings exposes real map/effect inspection, new-game and save/exit actions. The complete eight-page original illustrated walkthrough is adapted from `ToyBattleHowTo.cpp`: goal, turn, reach, covering, line home, regions, troop reference and special bases. Page positions/ownership/marks and text are the original source data. Both orientations and all eight page transitions are covered by native checks.
