# Provenance

Rules translated/adapted from CrossPlay (source checkout supplied by the owner); source revision `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Copyright notices and MIT grant are in `LICENSE-CrossPlay.txt`. This does not license unrelated third-party assets.

No external content bank or artwork is bundled.

No restricted Picross bank, Zachtronics dungeon bank or Underhand APK assets are included.


Sticky original UI parity (2026-11-16)

UI geometry and words follow immutable CrossPlay
`d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. Jersey glyph bytes and original card/
goods masks retain the included original MIT and font notices. Rules, AI, records,
save formats and compact UI are unchanged. Exact `legacy_*` UI copies preserve
621719e4 on unknown and nonSticky devices. New assets load only on Sticky's
existing enhanced (PSRAM-class) branch; constrained C3 remains on compact.lua.

Four 16x18 masks are exact center crops of the existing original 18x18 suit
art, matching native CardArt's clipped fan slot. The Hearts source forces CCW
landscape; the port follows it and restores the previous host orientation on exit.
The old core has one public-information heuristic, so the native Sharp/Rookie
switch is represented truthfully as TABLE: LOCAL. No second AI is fabricated.
A held completed trick advances after the original 900 ms UI delay using the
unchanged core action. Local records and all scored totals remain unchanged.
