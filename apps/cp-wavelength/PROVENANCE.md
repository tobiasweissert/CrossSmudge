# Provenance

Rules translated/adapted from CrossPlay (source checkout supplied by the owner); source revision `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Copyright notices and MIT grant are in `LICENSE-CrossPlay.txt`. This does not license unrelated third-party assets.

Data: all 245 pairs from `WavelengthPairs.h` / `tools_local/wavelength/pairs_en.txt`.

No restricted Picross bank, Zachtronics dungeon bank or Underhand APK assets are included.

## Original UI parity, local source freeze

The Sticky presentation follows immutable original `WavelengthScreens.cpp` at
`d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`: original 14/30/64 Jersey faces,
ink-centered line boxes, menu distribution, pass/pick/private/clue/ladder/call,
all distance verdicts, scoring table, pause, summary and help. Existing packed
font bytes and deck data remain unchanged. `art/pass92.raw` is only the original
Lucide pass-direction icon rendered by the actual source/SDK at its prescribed
92px contain size; source crop x36,y265,w92,h92. Existing Lucide and Jersey SIL
licenses apply. No live game or UI screenshot is substituted for drawing.

`legacy_graphics.lua`, `legacy_presentation.lua`, `legacy_enhanced.lua` are exact
baseline621719e4 modules for nonSticky/missing identity. Existing main.lua retains
its memory/release opt-in and unchanged compact path. Sticky follows original
portrait and restores entry orientation. Existing Co-op and Teams saved modes
remain playable via a small explicit footer adaptation: original Activity fixed
Teams, while the old port supports both. Existing game codec/journal/rules/deck
are unchanged. No RTC or new cross-boot owner claim is invented.

The old model has no lifetime histogram. UI-only `ui-observed-a/b.dat` slots
therefore store actual observations since this UI update, marked explicitly on
the menu/summary; old imported final states are fenced rather than counted as
new results. Practice never counts. Updates follow successful original game
journal saves, use readback checks, deduplicate completed states, preserve
records across new sessions, and saturate within original65535 counters. These
are observed UI records, not reconstructed historical data. Existing state-a/b
files remain untouched in format and location. Full-size production/native
pixels, callback tests, legacy preservation and exact limits are documented in
`test/wavelength_original_ui/README.md` and local frozen evidence.
