# Provenance

Adapted from the owner-supplied CrossPlay checkout, revision `4d40b952f52562e3816585c98bbbaf99d269d9d1`, `src/apps_local/seasalt/`. Original MIT notice is preserved in LICENSE-CrossPlay.txt. New Lua implementations and UI are adaptations, not original engine binaries.

`cards.lua` transcribes SeaSaltCards.h exactly. No artwork copied.

No restricted Picross bank, Zachtronics dungeon bank or Underhand APK assets are included.


Original Sticky UI parity (2026-11-16)

UI geometry, original words and licensed glyph/art pixels follow immutable
CrossPlay `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. The included MIT, Lucide and
Jersey OFL notices apply. Existing rules, AI, core, journals, records, IDs,
versions and icons are unchanged. Exact `legacy_*` presentation copies preserve
the 621719e4 nonSticky behavior; the 75 KiB dispatcher remains unchanged.
The rich UI uses the original portrait profile from both entry orientations and
restores the previous host orientation on exit. Native host pixels, production
Lua pixels, target compilation and physical-device acceptance are separate gates.

`ss_m_*_native20.raw` uses the original SDK Q8 Contain calculation: the 24px
colour icon draws 19px inside a 20px box. Original legacy 20px masks are retained.
The native card/keep/place/dig/table/selection/call/result vocabulary is drawn
from actual existing state and original glyph masks. A bot turn is presented as
one bounded sequence of the same old engine decisions; narration uses only
observed actual card/pair/call changes and clears on a new game/round. Existing
discard confirmation remains as a save-protection adaptation; the original
starts a new game immediately. PLAY NEARBY shows an explicit unavailable notice
because this Lua host has no original peer backend. No peers are fabricated.
