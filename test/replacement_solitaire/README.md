# Solitaire replacement verification

Run `python3 test/replacement_solitaire/run.py`.
The optional `--source /path/to/crossplay` uses an external checkout instead of the exact vendored oracle. `--app-dir /path/to/extracted/app` verifies candidate ZIP contents.

The optional `--output /path/to/result.json` records source and Lua configuration hashes and results.

This compiles the repository's exact vendored Lua VM (signed 32-bit integer,
32-bit float) and the *original* CrossPlay `SolitaireCore.cpp` in a temporary
directory. It does not build firmware or fetch an SDK. Each test directory owns
its runner and has no dependency on another replacement PR.

The native oracle emits 2,652 deal/move/stock/recycle/undo snapshots using six
seeds (including zero and high-bit uint32 seeds), both draw modes and 220 moves
per game. Lua must reproduce every card, face direction, pile, move count, win
and auto-finish predicate. Extra tests cover undo depth, an entire auto-finish,
bounds, 52-card conservation, save round trips and every truncation of a save.

`legacy_regression.lua` is a pre-replacement regression: the original callback
silently ignores stock-signature `(x,y,event)` taps. It runs only before the new
core exists; current input coverage must exercise the replacement's rendered
stock location through its public callbacks.

The native renderer adapter is not evidence of on-device refresh speed, timing,
render quality or physical touchscreen routing. Hardware acceptance requires
opening Solitaire on Sticky with buttons and touch, dealing in
both draw modes, tapping stock, moving/undoing runs, finishing a revealed game,
leaving/resuming, and interrupting a save. A legacy save must remain unchanged
and show the preservation notice. No EPUB cache reset is necessary.

The SVG artifacts record actual `on_draw` calls using approximate font metrics and header chrome. They are explicitly adapter visualizations, not production renderer screenshots. The runner maps only the app save directory and CrossPlay import directory into temporary fixture directories.

The 75 KiB runs include 120 consecutive stock actions with saving and drawing. The richer repeated-load integration runs at 2 MiB because its assertion bytecode and simultaneous module reloads are test-only overhead.


## Sticky-only integration gate

`package.py --test` also runs `preservation.py` against extracted ZIP bytes. It checks every original module/asset against pinned git base 2c82f306, compares actual callbacks/render calls/save values and reload behavior for x3/x4/x4pro/simulator/unknown/missing API in both orientations, and forbids cp modules on those paths. Legacy trace comparison uses 2 MiB on the host; separate 75 KiB baseline/candidate observations record instrumentation/host-pointer limits honestly. Existing replacement entry tests explicitly select Sticky; all source-oracle assertions remain intact. Earlier replacement-only C3 stress tests are not the device dispatcher behavior.

`touch.lua` invokes direct stock callbacks for card/run selection, deselect, empty targets, invalid/face-down moves, foundation transfers, crowded 24-card overlap, touch-only reset/cancel, recycle, Undo and Finish in both orientations. Its deselect regression failed before the Sticky controller correction.
