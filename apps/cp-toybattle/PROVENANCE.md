# Provenance

Adapted from the owner-supplied CrossPlay checkout, revision `4d40b952f52562e3816585c98bbbaf99d269d9d1`, `src/apps_local/toybattle/`. Original MIT notice is preserved in LICENSE-CrossPlay.txt. New Lua implementations and UI are adaptations, not original engine binaries.

`maps/*.lua` exports the source Terrain definitions via test/crossplay_ports/export_toy.cpp; no printed artwork copied.

No restricted Picross bank, Zachtronics dungeon bank or Underhand APK assets are included.

`geometry.lua` preserves source normalized slot/region medal coordinates from `ToyBattleCore.cpp`; the original MIT notice covers these renderer data. No printed board artwork is included.

`lesson_data.lua` preserves the original eight-page teaching positions and captions from `ToyBattleHowTo.cpp`, covered by the retained MIT notice.


Original Sticky UI parity (2026-11-16)

UI geometry, original words and licensed glyph/art pixels follow immutable
CrossPlay `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. The included MIT, Lucide and
Jersey OFL notices apply. Existing rules, AI, core, journals, records, IDs,
versions and icons are unchanged. Exact `legacy_*` presentation copies preserve
the 621719e4 nonSticky behavior; the 75 KiB dispatcher remains unchanged.
The rich UI uses the original portrait profile from both entry orientations and
restores the previous host orientation on exit. Native host pixels, production
Lua pixels, target compilation and physical-device acceptance are separate gates.

`mark-*.raw` and `special-*.raw` are original `drawTroopMark`/`drawSpecialGlyph`
component masks rendered natively, including the original XB42 white knockout.
`help-diagram-*.raw` are immutable original walkthrough illustrations/reference
material; their chrome, captions and controls remain live. `brief-*.raw` are the
original fixed map/base-option reference plates. Playing board/rack/medals/gates,
miniatures, prompts, menus, setup/map choices and results remain live values and
geometry. Regeneration scripts live in test/seasalt_toybattle_original_ui.

`ui_draft.lua` owns only the original question/selection presentation: answers
and completion probes go through unchanged `M.apply`; actual state changes only
on commit. Projected pieces do not mutate the saved game. Optional base effects
remain deferred by the existing engine, including Captain chains.

The old model has one local AI and first-player solo setup, so OPPONENT: LOCAL
and YOU MOVE: FIRST are fixed fields; no unsupported choices are registered.
It has no lifetime history: fresh sessions can truthfully show NO BATTLES YET;
loaded/completed games show LOCAL GAME/LOCAL GAME COMPLETE. No past counts are
invented. PLAY NEARBY reports the missing host backend and keeps the local game.
These exact data gaps are excluded from claims of native data equivalence.
