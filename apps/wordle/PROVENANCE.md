# Provenance

CrossPlay source checkout: `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Rules/engine files below were read locally without modification; their content hashes pin the actual inspected source.

CrossPlay MIT notices: 2025 CrossPoint Reader organization; 2026 Mario Ruiz. Full license: LICENSE-CrossPlay.txt. Lua translation and stock-host adaptation are new. Original CrossSmudge app attribution remains Mumfee; repository MIT notice is 2025 Dave Allie (LICENSE-CrossSmudge.txt).

| Source | SHA-256 |
| --- | --- |
| `src/apps_local/wordle/WordleCore.cpp` | `47b7c551f9f89829f639927ef8711160d649246f04b80aa81cb38aa27dfe424d` |
| `src/apps_local/wordle/WordleCore.h` | `650c3f06cbb8d886e86119e14a74122df9d525ee6d1c3adf8a9d10953f80cb41` |

Preserved CrossSmudge assets: `icon.raw`, `words.bin`. CrossPlay native UI/fonts are not copied.

Inherited words.bin SHA-256: `7479dd0c333009f2be3649fb775e2f7497ab72d1a81a12d3456825b329e1ea4d`. No new third-party word archive is redistributed.

The original main is retained verbatim as `legacy_main.lua`; original dependencies and assets retain their original bytes. Only Sticky dispatches through `cp_app.lua`.
