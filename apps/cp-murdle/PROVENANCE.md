# Provenance

Adapted from the owner-supplied CrossPlay checkout, revision `4d40b952f52562e3816585c98bbbaf99d269d9d1`, `src/apps_local/murdle/`. Original MIT notice is preserved in LICENSE-CrossPlay.txt. New Lua implementations and UI are adaptations, not original engine binaries.

`data/*.lua` contains 64 reproducible original-generator cases. test/crossplay_ports/generate_murdle.py invokes the original generator, uniqueness solver, deduction solver, cast and text code; no external case bank.

No restricted Picross bank, Zachtronics dungeon bank or Underhand APK assets are included.

The Sticky presentation is now mapped to the immutable original native builders
at `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b` (the game files equal the prior
reference). `test/murdle_original_ui/build_native.py` compiles original Murdle
Screens/Core/Cast/Text plus the pinned renderer and original glyph cuts. Its
fixture recreates the original puzzle from each exported seed and restores the
actual entered marks through `Marks::enter`; it does not paint a guessed board.
All64 cases use the existing bank unchanged. `assets.py` repacks the original
fixture sentence phrases for live accusation text; its hash/license receipt is
`art/phrases-source.json`. No case solutions or rules were changed.

`legacy_enhanced.lua` and `legacy_graphics.lua` preserve baseline621719e4 bytes.
The original presentation activates only on Sticky. Missing identity and every
other device retain the previous high-heap view; low-heap routing is unchanged.
