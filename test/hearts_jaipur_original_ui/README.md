Hearts / Jaipur original UI

Run `python3 test/hearts_jaipur_original_ui/all.py --output /tmp/card-ui-evidence`.
Requires the pinned owner CrossPlay checkout at `/root/github/crossplay` and the
frozen native scaffold in `.host-data/crosssmudge/ported-ui-parity/native-catalog-references`.
Builds fresh private native objects and the shared canonical production Lua
adapter; never runs PIO or modifies either reference. Original compilation units
are checked byte-for-byte against d3a49ce8 before compilation.

`baseline.lua` touches every help page and Jaipur's honest unavailable Nearby
notice. `interaction.lua` uses actual touch/update callbacks to play a complete
Hearts game and Jaipur match, select/undo three-card passes, reject illegal cards,
trade multiple market cards using the herd, select/undo camels, inspect the held
round-ending board, open results, continue rounds, exit, and reopen the exact save.
Both entry orientations run at the real Sticky identity and 2 MiB Lua budget.
Hearts forces the original landscape_ccw profile; Jaipur forces portrait.
Orientation restoration is asserted, so portrait-native content is never clipped
into a landscape-shaped viewport.

`verify.py` compares every pixel of the supported source-model fixture set:
Hearts passing/selection/playing/trick/hand-over/game-over and all four help pages;
Jaipur opening menu/gameplay and all eight help pages. Hearts menu is exact outside
its declared TABLE: LOCAL adaptation (old AI has one strength). Tests do not claim
Jaipur's aggregate results are identical to the native per-good payout ledger.
`pixels.json` states each full-frame comparison and the sole excluded menu region.
Jaipur later trade/camel/end/round/game-over callbacks produce full PNGs/traces;
they have behavior assertions but do not yet have native mapped-state pixel gates.

`preservation.py` compares actual immutable 621719e4 and current callback pixels,
primitive traces and save bytes for high-heap x4-pro and 75 KiB x3, both entry
orientations. C3 text primitives are traced through the host adapter because the
bitmap seam has no built-in text renderer; no C3 text pixel approval is claimed.
`red_baseline.py` records the immutable port's actual native pixel failures.

`native.py` reuses the frozen production/native renderer class scaffold, real
original fonts and primitives, then compiles the original Screens and CardArt.
Its Hearts adapter translates actual Lua encoded card state into the original
native model; it does not substitute a different native seeded hand. Its scope is
1-bit glyphs/ordinary text used by these fixtures, not device e-ink waveform or
touch calibration. No hardware claim follows from a host render.

`create tutorial assets`: run `tutorial_assets.py --native <native executable>
--output <evidence directory>` only when intentionally regenerating licensed art.
Seven immutable original Jaipur tutorial illustrations are 448x280 masks, cropped
from actual original Screens output. Titles/captions/rules/footer/navigation are
live text and controls. The port caches just the active 15,680-byte illustration.
The eighth page is fully live original small-print text. See app provenance.

Hardware: on Sticky open Hearts in either orientation, check CCW table, pass
selection, gray illegal cards, the 900 ms held trick, scores, exit/reopen. Open
Jaipur in either orientation, take/undo camels, trade two goods using hand/herd,
check held last board -> SEE SCORES -> NEXT ROUND, then exit/reopen. x3/x4 and
x4-pro retain the baseline UI; no save/cache reset is required.
