# Original CrossPlay Chess presentation

`python3 test/chess_graphics/assets.py /path/to/CrossPlay-reference` repacks original Jersey fonts and Celtic masks, retaining their licenses and SHA256 provenance. Black-mask polarity is converted losslessly from bit-one ink to Smudge's bit-zero ink. Both source sizes remain native; no runtime resampling.

`python3 test/chess_graphics/run.py` builds the app-specific replacement Chess adapter against the vendored firmware int32/float32 Lua. Tests exercise portrait/landscape board geometry, all supported settings targets and all four promotions; independent expected SAN fixtures cover castling, disambiguation, en passant, promotion and mate, and assert notation does not mutate the position. Captures replay actual piece/font masks for initial board, captured strips/SAN history, settings and promotion. Evidence goes to `/tmp/chess-ui-results.json` and `/tmp/chess-ui-captures/`.

The broader `test/replacement_chess/run.py` source-oracle suite remains authoritative for engine, journal and callback regressions. Its draw doubles now include bitmap/rounded_rect; its journal fakes delegate app-private `art/` reads to the bounded native reader. Original journal behavior/failure assertions remain in force. The capture adapter now records actual bitmap glyphs as well as piece pixels.

Native caps describe only the Lua allocator. Captures are draw-call replays, not physical e-ink measurements. See `apps/chess/PARITY.md` for the remaining original UI differences and hardware checks.
