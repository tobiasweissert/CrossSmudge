# Chess replacement verification

Run `TMPDIR=/root/.hermes/cache/scratch python3 test/replacement_chess/run.py` from the firmware repository. `oracle/` vendors the exact original CrossPlay core and engine plus their MIT license. `CROSSPLAY_ROOT` optionally selects an external read-only checkout instead. `REPLACEMENT_EVIDENCE` selects the report output directory.

This test directory is independent of the other replacement PRs and the unmerged `cp-*` apps. Its native runner is included here; it builds only the repository's vendored Lua, whose configuration uses signed 32-bit integers and 32-bit floats. It does not build firmware or download the SDK.

`oracle.cpp` links the original CrossPlay `ChessCore.cpp` and `ChessEngine.cpp`, with no implementation substitution. It produces 72 fixtures: 12 special positions and 60 positions from three seeded random games. `compare.lua` checks exact legal move ordering and flags, each child's FEN, undo restoration, perft, check, evaluation, and search results (move, score, nodes, budget flag) at depths 1 and 2 with unlimited and 17-node budgets. Starting-position perft at depth 3 must be 8,902. Source hashes are recorded in the JSON report. All 128 square/colour attack queries are compared per position; depth 0/9 clamping and depth 4 are also checked with a 17-node budget.

`invariants.lua` independently exercises eight longer seeded games, legal king safety, make/unmake and FEN round trips, plus a complete Fool's Mate. These are rules checks, not a claim that arbitrary random play always finishes within 400 plies.

The native drawing adapter validates coordinates and API coverage; it is not the physical display, SD filesystem, or a device performance measurement. Hardware checks remain necessary: on Sticky open Chess, play/undo, change AI mode, promote, sleep/resume and relaunch. On X3/X4 verify original gameplay and save resume. Do not clear old saves to test migration.

The runner was adapted from the project's CrossPlay ports harness and is carried independently here under the repository's MIT license. The source oracle uses CrossPlay's MIT-licensed engine at the path configured above. Source checkout: https://github.com/ma-r-s/crossplay at `4d40b952f52562e3816585c98bbbaf99d269d9d1`; the four chess core/engine source files were clean in that checkout. The default oracle is vendored, so no sibling checkout is required; the test never clones or changes external source.

`promotion.lua` also runs against the exact existing-app baseline engine from commit `2c82f306`, vendored under `fixtures/` with the repository license: that run must fail with one promotion choice. The replacement must pass with four. The initial missing-port RED evidence was recorded before implementation; the later baseline behavioral comparison is not presented as having preceded implementation.

`validation.lua` rejects structurally invalid and impossible persisted board states while accepting legal check/checkmate states. `journal.lua` injects corrupted slots, torn writes, I/O failures and readback mismatch, and verifies bounded access and preservation of original/prior-backup legacy bytes. Incremental search is compared with the same original-source results, and each step/cancellation checks that the displayed position was never changed.


Replacement-only stress limits: with Sticky explicitly selected, this translated core does **not** support gameplay at a synthetic 75 KiB Lua budget. The suite deliberately retains the expected out-of-memory rules probe as evidence, and separately requires safe refusal before the engine loads (no save reads or writes). The pre-guard full-app failure is retained in `chess-c3-bottleneck.json` outside the repository. At 2 MiB, both orientations run the actual game and lifecycle tests. Do not summarize the expected OOM or the refusal screen as passing C3 gameplay.

`lean.lua` tests the supported launch/move/cancel/exit path or the replacement low-memory guard. `runtime.lua` covers button press/release, rendered touch targets, complete Fool's Mate, all-choice promotion UI (selecting a knight), an actual AI answer, cancellation/resume, undo, persistence and twelve reloads. `history.lua` rejects checksum-valid but noncontiguous saved positions which could otherwise manufacture a draw. `capture.lua` exports production drawing calls to SVG using real piece bitmap pixels and a substitute text font; these are explicitly native adapter views, not device screenshots.

To test package bytes, extract a candidate ZIP and set `CHESS_APP_DIR` to its app directory. The report records every tested app-file SHA-256. Temporary build/fixture files live under `TMPDIR` and are removed when the test exits.


## Sticky-only integration gate

`package.py --test` also runs `preservation.py` against extracted ZIP bytes. It checks every original module/asset against pinned git base 2c82f306, compares actual callbacks/render calls/save values and reload behavior for x3/x4/x4pro/simulator/unknown/missing API in both orientations, and forbids cp modules on those paths. Legacy trace comparison uses 2 MiB on the host; separate 75 KiB baseline/candidate observations record instrumentation/host-pointer limits honestly. Existing replacement entry tests explicitly select Sticky; all source-oracle assertions remain intact. Earlier replacement-only C3 stress tests are not the device dispatcher behavior.
