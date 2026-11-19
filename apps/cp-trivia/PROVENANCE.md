# Trivia presentation provenance

The Sticky UI adapts `src/apps_local/trivia/TriviaScreens.cpp` and its direct Toybox collaborators from `ma-r-s/crossplay` at immutable `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. CrossPlay's MIT notices are retained in `LICENSE`.

The original `TriviaActivity.cpp:1119–1128` binds `readingChromeFaces()` for Notice and Solo: Jersey 14 for options/footer labels, Noto Serif 14 for question prose, and Jersey 30 for the title. The three original 1-bit cuts are repacked losslessly, including all ASCII/Latin-1 glyphs in their source intervals, original 12.4 advances, signed bearings, and Noto's original 36×30 kerning matrix. `test/trivia_ui/repack.py` reads only immutable Git objects and records source/output SHA-256 hashes in `art/sources.json`. This is bitmap repacking, not font regeneration.

Jersey 25 is distributed under the SIL Open Font License in `OFL-Jersey25.txt`. Noto Serif is distributed under the SIL Open Font License in `OFL-NotoSerif.txt`.

`legacy_main.lua` preserves the complete entry point from CrossSmudge baseline `621719e4f68d8ddf719d470f84be20ad22c11d35`; missing device API and every non-Sticky device dispatch to that unchanged implementation. `model.lua`, `cache.lua`, `json.lua` and legacy `ui.lua` remain unchanged.

The provider remains Open Trivia DB, with no question pack, invented score, difficulty, game mode, records, reporting or settings. Choice and Notice retain the original composition for supported data. Explicit differences are required by the existing provider/runtime: the existing Skip action occupies the original otherwise empty primary footer; cache/request status uses the otherwise empty difficulty strip (or native header-right slot in landscape); unsupported HIDE is omitted, retaining NEXT/END; long provider text opens complete paged question/answer cards through visible READ and shortened-answer controls; landscape reduces option rows where the native builder leaves a negative clue height. Original fonts lack arbitrary Unicode; unsupported codepoints are displayed as `[U+XXXX]` using the licensed masks while exact provider strings and answer comparison stay unchanged.

These limitations are explicit, not a claim of complete original application parity or physical-device acceptance. The bounded host capture uses production raster methods and original mask bytes; hardware qualification remains separate.
