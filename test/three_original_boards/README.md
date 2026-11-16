These tests cover only Connect Four, Knucklebones and Checkers. The production
runner compiles the actual Sticky device binding, essential Lua libraries,
production shape primitives and sprite rasterization. It has no host font
fallback. Each Sticky VM runs within 2 MiB and must free every allocation.

`run.py` runs one fixture against a source tree, or extracted packages supplied
as `--app-root` containing the three `cp-*` directories. `qualify.py` runs all
seven fixtures. Both take `--output`, `--tooling` (the directory produced by
`test/crossplay_ui_parity/build_capture.py`), `--native` (frozen original catalogue
executable) and `--extra-native` (the bounded executable built by
`build_native.py --output <evidence-directory>`).

The original reference is CrossPlay d3a49ce8. The extension copies the frozen
native renderer backend unchanged and calls real original screen builders.
Initial models use the frozen catalogue. Other models receive the actual
production board, turn, die, selected square, idle count and move history.
Original rule helpers calculate legal destinations, remaining pieces and
winning lines. The results fixture generates legal core play and loads each
outcome through the real app journal; gameplay/lifecycle fixtures additionally
complete real games through production touch/button/update callbacks.

Portrait normal menus, gameplay samples, all nine lesson pages, all nine
result variants, final-position ornaments and 65,535-count menus require zero
whole-frame pixel differences. Menus also match in landscape. Landscape
boards/lessons/ornaments adapt the original fixed portrait geometry to fit;
their native differences remain recorded and do not count as pixel identity.
Nearby explicitly reports the unavailable Lua host service and retains the
existing local two-player/new-solo actions. Record-write failures add an error
notice; that notice has no equivalent field in the original menu model.

`nonsticky.py --output <directory>` compares baseline/current API-call digests
at 75 KiB and 2 MiB, both bounds and four explicit non-Sticky identity fixtures.
It also checks the fifteen legacy modules against the exact baseline bytes.
That host-adapter regression does not claim non-Sticky physical screenshots.

Counters belong to an optional app-relative two-slot checksum journal. They
count only running-to-complete transitions observed by this UI. Existing
terminal saves do not fabricate lifetime totals. Core journals, game rules,
AI, old fonts, manifests and versions are unchanged.

For hardware verification, copy each complete app directory (including the
new assets and legacy modules) to Sticky, retaining its existing state files.
Open Apps > Connect Four / Knucklebones / Checkers in both orientations. Check
Play/How to Play/all three lessons, tap a legal move, wait for the opponent,
finish a game, use Done and Play Again, reopen it, and check its final-board
ornament and record. Play Nearby must show the actual unavailable-service
message; local two-player must never run the AI. Retry after an induced storage
write failure and confirm the game remains present. Verify the former UI on
X3/X4 and X4 Pro as a separate hardware check. No cache reset or save deletion
is required. Physical hardware was not exercised by this host suite.
