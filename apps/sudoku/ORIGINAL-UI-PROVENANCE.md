Original CrossPlay UI sources and Jersey25 glyph cuts are pinned to
ma-r-s/crossplay d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b.
`art/original-sources.json` records exact source and asset SHA256 values.
The glyphs are lossless1-bit repacks preserving original bearings and12.4 advances;
`OFL-Jersey25.txt` accompanies them. Reproduce with
`test/utility_sudoku_original_ui/repack.py`.

Rendering follows original sudoku/SudokuScreens.cpp and default Toybox font-slot bindings.
Exact model mappings, reference pixels, scoped exclusions and limitations are in
`test/utility_sudoku_original_ui/reference/sources.json` and its routine README.
Rules, core, journals and imported bytes are unchanged. Additional compact font
strings load once on Sticky; no per-glyph object cache or second framebuffer.
The original legacy paths retain their exact old bodies on every other device.
