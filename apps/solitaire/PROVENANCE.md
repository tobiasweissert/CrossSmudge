# Provenance

CrossPlay source checkout: `4d40b952f52562e3816585c98bbbaf99d269d9d1`. Rules/engine files below were read locally without modification; their content hashes pin the actual inspected source.

CrossPlay MIT notices: 2025 CrossPoint Reader organization; 2026 Mario Ruiz. Full license: LICENSE-CrossPlay.txt. Lua translation and stock-host adaptation are new. Original CrossSmudge app attribution remains Mumfee; repository MIT notice is 2025 Dave Allie (LICENSE-CrossSmudge.txt).

| Source | SHA-256 |
| --- | --- |
| `src/apps_local/solitaire/SolitaireCore.cpp` | `8448b5264d4073908f5d39485763fba4d4fb40576b086e8b65eba2992389ae19` |
| `src/apps_local/solitaire/SolitaireCore.h` | `2bb6cf8772c7344926eb573089f8ba01ebc6c57d9ca4c84e962aefe03f62867e` |
| `src/apps_local/cards/Cards.h` | `47b9b44c4d2edec5c717b096525d5cb51be51a41853e69926fb32e505fdcee39` |

Preserved CrossSmudge assets: `icon.raw`, original suit bitmaps (repacked losslessly). CrossPlay native UI/fonts are not copied.

The original main is retained verbatim as `legacy_main.lua`; all original dependency and asset paths retain their original bytes. Sticky replacement code is dispatched through `cp_app.lua`. Solitaire keeps the packed replacement suit artwork separately in `cp_suits.lua`, with the original `suits.lua` unchanged.
