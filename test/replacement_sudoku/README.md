# Sudoku replacement verification

Run `TMPDIR=/root/.hermes/cache/scratch python3 test/replacement_sudoku/run.py` from the repository. The harness compiles only the repository's vendored Lua VM and a freestanding C++ rules oracle. No firmware build, SDK, network, or other replacement PR is required. The VM verifies signed 32-bit integers and 32-bit floats, tracks live allocations, enforces a 2 MiB ceiling, and verifies zero tracked bytes after closing.

`oracle/SudokuCore.{h,cpp}` are unmodified MIT-licensed snapshots from CrossPlay commit `4d40b952f52562e3816585c98bbbaf99d269d9d1`; those source paths were clean when copied. The adjacent license is retained. SHA-256:

* `SudokuCore.cpp`: `4e73036f80ff1eb09dc6a5b5f2e60b15ee8b8d113141bdd1ee1ea4100acc3fa9`
* `SudokuCore.h`: `1eddd0672c092afe05fb10b70fb534704e889981f8d3fc787cda65610f27f8ff`

The C++ oracle generates four puzzles in each source difficulty band and runs each through all eleven technique ceilings. The Lua implementation must agree on every resulting value, solved/broken status, hardest technique, and solver step count. This is deterministic cross-language comparison, not merely assertions that a puzzle can be solved.

The next placement hint is also compared in each case, including the technique and highlighted unit. The generator reproduces all sixteen successful C++ seeds exactly, including clue masks, solutions, and measured grades; a separate MRV backtracking solver then counts solutions up to two. The generator test prints the slowest native CPU duration without asserting this establishes device latency.

`legacy_regression.lua` was first run before production changes with the exact VM and failed on the original generator's missing rotational clue symmetry at seed 12345. A subsequent stronger check against the still-unchanged original generator proved it also produced multiple solutions. Use `--legacy-regression` only against that original generator; it is a recorded red regression, not part of the replacement's green suite.

Journal tests try every truncation and one-byte mutation of a completed save, failed and partial writes, readback, and byte-preserving legacy backups. Oversized legacy originals remain intact, and an existing backup is never replaced.

Production runtime tests cover portrait and landscape input, incremental generation cancellation, keypad pencil mode, undo, wrong-entry hints, a complete played game, paused and persisted elapsed time, write debouncing, semantically invalid saves with valid checksums, and twelve reloads. The explicitly Sticky-selected 75 KiB stress launch must display the low-memory screen without accessing saves. Non-Sticky dispatch loads the original game. A separate engine-only probe records the actual C3 allocation failure; the guard passing is not C3 gameplay support.

When `--output path/to/results.json` is supplied, the harness saves two `sudoku-adapter-*.svg` files alongside it. These replay production `on_draw` calls with approximate SVG fonts and header/hint rendering; they are not device framebuffer captures. `--app-dir` can point at an extracted candidate ZIP to test the delivered bytes.

The C API adapter came from CrossSmudge's existing `test/crossplay_ports/runner.c` and is independently included here under the repository MIT license. It is a native test adapter, not an on-device renderer or performance measurement. Test logic uses only app-owned files and standard Lua. Draw-coordinate checks do not establish visual quality.


## Sticky-only integration gate

`package.py --test` also runs `preservation.py` against extracted ZIP bytes. It checks every original module/asset against pinned git base 2c82f306, compares actual callbacks/render calls/save values and reload behavior for x3/x4/x4pro/simulator/unknown/missing API in both orientations, and forbids cp modules on those paths. Legacy trace comparison uses 2 MiB on the host; separate 75 KiB baseline/candidate observations record instrumentation/host-pointer limits honestly. Existing replacement entry tests explicitly select Sticky; all source-oracle assertions remain intact. Earlier replacement-only C3 stress tests are not the device dispatcher behavior.

The Sudoku native dofile adapter now mirrors stock LuaRunner.cpp full GC before compilation and after execution; this fixes a harness-only memory discrepancy exposed by the dispatcher test.
