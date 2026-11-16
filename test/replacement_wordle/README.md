# Wordle replacement verification

Run `python3 test/replacement_wordle/run.py`.
Use `--source /path/to/crossplay` to compare against an external checkout instead of the vendored oracle; `--app-dir /path/to/extracted/app` tests candidate ZIP contents.
Use `--output /path/to/result.json` to retain source/configuration hashes and results.

The standalone runner compiles this repository's vendored signed-int32/float32
Lua and original CrossPlay `WordleCore.cpp`; no SDK or firmware build is used.
The differential checks compare 1,000 repeated-letter scores, 250 randomized
on-the-day statistics records, and 1,001 day/date round trips against the C++
source. Additional checks exercise short/rejected guesses, typed-row retention,
key mark precedence, wins/losses, terminal input, recomputed saves and malformed
records. The tests do not rely on another replacement's PR or harness.

CrossPlay does not ship a word bank. Its native activity downloads an answer
archive and sorted accepted guesses from mfilej/wrdl and updates recent days
from NYT. The Lua port must truthfully distinguish those supplied archive files
from CrossSmudge's existing random-practice `words.bin`; this test does not
license, fetch or claim provenance for external word-bank content.

Native adapter output does not demonstrate device rendering or timing. On real
Sticky, test keyboard/buttons, invalid guesses, repeated-letter
marks, archive day navigation and dated statistics, exit/resume and interrupted
writes. Confirm any old save remains untouched and that no invented daily word
appears when the dated archive is unavailable. No EPUB cache reset is needed.

The SVG artifacts record actual `on_draw` calls using approximate font metrics and header chrome. They are explicitly adapter visualizations, not production renderer screenshots. The runner maps only the app save directory and CrossPlay import directory into temporary fixture directories.

The 75 KiB runs cover launch, 200 keyboard actions, a dated win, the complete permitted 1,024-day compact result span, and safe refusal of larger histories without modifying their journals. Leading zero result days are omitted with a stored offset. The maximum 8,192-day results fixture runs under the 2 MiB Sticky budget; the Sticky-selected constrained harness preserves that history and offers practice with an explicit PSRAM notice. Large integration scripts also run at 2 MiB to avoid counting test assertion bytecode and simultaneous reloaded app closures as production memory.


## Sticky-only integration gate

`package.py --test` also runs `preservation.py` against extracted ZIP bytes. It checks every original module/asset against pinned git base 2c82f306, compares actual callbacks/render calls/save values and reload behavior for x3/x4/x4pro/simulator/unknown/missing API in both orientations, and forbids cp modules on those paths. Legacy trace comparison uses 2 MiB on the host; separate 75 KiB baseline/candidate observations record instrumentation/host-pointer limits honestly. Existing replacement entry tests explicitly select Sticky; all source-oracle assertions remain intact. Earlier replacement-only C3 stress tests are not the device dispatcher behavior.
