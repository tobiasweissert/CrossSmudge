Original UI: ma-r-s/crossplay d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b,
ChessScreens.cpp, ChessActivity.cpp and the pinned FreeInkUI/Toybox theme.
`art/original-sources.json` pins Jersey source hashes and exact repacked glyph
bytes/bearings/12.4 advances; `OFL-Jersey25.txt` travels with them. Existing
approved Celtic piece masks and legacy sprite files are unchanged.
`art/nearby-source.json` pins the unchanged ToyboxIcons nearby32 symbol;
`LICENSE-Lucide.txt` accompanies it.

Pixels/model mappings and limits are pinned in
`test/chess_original_ui/reference/sources.json`. The routine README lists exact
full/scoped states, the2extra supported settings actions, fitted landscape piece
cut, unavailable Nearby backend, absent F2F saved field, preserved target
underpromotion and session-only Move Hints. No chess engine, journal, rules,
approved piece sprites, app ID/version or catalog is changed.

Compact font strings load once per VM; no per-glyph table or second framebuffer.
The existing main guard keeps non-Sticky legacy code and the75KiB unsupported
Sticky path before this UI is loaded. Physical device qualification remains
separate from production-method host pixels and callback verification.
