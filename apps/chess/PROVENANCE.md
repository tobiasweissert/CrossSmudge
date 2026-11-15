# Provenance

CrossPlay source checkout: `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Rules/engine files below were read locally without modification; their content hashes pin the actual inspected source.

CrossPlay MIT notices: 2025 CrossPoint Reader organization; 2026 Mario Ruiz. Full license: LICENSE-CrossPlay.txt. Lua translation and stock-host adaptation are new. Original CrossSmudge app attribution remains Mumfee; repository MIT notice is 2025 Dave Allie (LICENSE-CrossSmudge.txt).

| Source | SHA-256 |
| --- | --- |
| `src/apps_local/chess/ChessCore.cpp` | `4eb0ad7530f5435f69f78bd8b1336b4e19d277979d25ff9489571faa6535d7ed` |
| `src/apps_local/chess/ChessCore.h` | `66dd007f3a629cbfe53dc1ce40472100b7a674b78de1c9504c5da9393e765b30` |
| `src/apps_local/chess/ChessEngine.cpp` | `212b77f1fecb75ecae9cba8d9019f1e3aca389d9ad694367d6ec667c5ed67674` |
| `src/apps_local/chess/ChessEngine.h` | `8b25190c72f02921f0b7e19deeb8189531c344015bf42db687d524908d331016` |

Preserved CrossSmudge assets: `icon.raw`, `sprites/*.raw`. CrossPlay native UI/fonts are not copied.

The original main is retained verbatim as `legacy_main.lua`; original dependencies and assets retain their original bytes. Only Sticky dispatches through `cp_app.lua`.
