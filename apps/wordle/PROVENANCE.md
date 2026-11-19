# Provenance

CrossPlay source checkout: `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Rules/engine files below were read locally without modification; their content hashes pin the actual inspected source.

CrossPlay MIT notices: 2025 CrossPoint Reader organization; 2026 Mario Ruiz. Full license: LICENSE-CrossPlay.txt. Lua translation and stock-host adaptation are new. Original CrossSmudge app attribution remains Mumfee; repository MIT notice is 2025 Dave Allie (LICENSE-CrossSmudge.txt).

| Source | SHA-256 |
| --- | --- |
| `src/apps_local/wordle/WordleCore.cpp` | `47b7c551f9f89829f639927ef8711160d649246f04b80aa81cb38aa27dfe424d` |
| `src/apps_local/wordle/WordleCore.h` | `650c3f06cbb8d886e86119e14a74122df9d525ee6d1c3adf8a9d10953f80cb41` |

Preserved CrossSmudge assets: `icon.raw`, `words.bin`. The Sticky presentation now adapts the pinned original native UI and bundles licensed original glyph masks; details below.

Inherited words.bin SHA-256: `7479dd0c333009f2be3649fb775e2f7497ab72d1a81a12d3456825b329e1ea4d`. No new third-party word archive is redistributed.

The original main is retained verbatim as `legacy_main.lua`; original dependencies and assets retain their original bytes. Only Sticky dispatches through `cp_app.lua`.


## Original UI assets and source

UI reference: CrossPlay `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`; Wordle/Connections renderer inputs equal its upstream parent `4d40b952f52562e3816585c98bbbaf99d269d9d1`. The source menu, guess chart and help are in `src/apps_local/wordle/WordleScreens.cpp`; the archive borrows `src/apps_local/connections/ConnectionsScreens.cpp` with `serifMenuFaces()` as selected by `WordleActivity::render()`.

`art/font{10,20,30}.bin` are lossless ASCII masks from the original Jersey25 cuts, now including lowercase for the original help text. `art/serif{10,13}.bin` and corresponding `-steps.bin` contain original Instrument Serif masks and exact differential-rounded ASCII kerning steps. Both families are SIL OFL; retain `OFL-Jersey25.txt` and `OFL-InstrumentSerif.txt`. Source hashes are in `art/sources.json`; `test/wordle_original_ui/assets.py` checks immutable source bytes and reproduces these assets. Existing Lucide key assets/notices remain intact.

Portrait menu, help, archive and real statistics use the original composition and exact glyph pixels. The original tall menu/calendar do not fit landscape; the port retains those controls and assets while placing chart/list or steppers/calendar beside each other. The native streaming remote-bank importer is not exposed by the existing offline app. Opting this manifest into Lua networking changes its host storage domain (`LuaRunner.cpp`), so that service needs separate save-preserving integration. Its UI action is explicitly replaced with the existing SD import route (`wordle.ans`/`wordle.words`, copy then reopen); no puzzle bank or download service is fabricated. IDs, old saves, results and the original non-Sticky route are preserved. `cp_app.lua` exports its already-existing save-safe `begin` function so real calendar taps can open a date through the same controller.
