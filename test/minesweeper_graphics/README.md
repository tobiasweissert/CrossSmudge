# Minesweeper graphics and opt-in hold tests

Run `python3 test/minesweeper_graphics/run.py` and `input.py` from the firmware
repository. Evidence is written only to the dedicated minesweeper-graphics-hold
folder. Both compile the shipped int32/float32 Lua. No production save files are
opened. `assets.py` regenerates licensed bitmap assets from the local read-only
CrossPlay checkout; source hashes and licenses ship with the app.

`run.py` executes production Lua on_draw against a logical framebuffer adapter.
Its row-major, zero-bit-is-ink transparent blit follows LuaRunner::l_drawSprite;
rect borders and LightGray follow GfxRenderer logical semantics. Actual shipped
Jersey glyph bytes are repacked losslessly from continuous MSB-first Epd pixels
to row-padded Lua sprite pixels, with native advance/bearing/cap metrics. No
synthetic SVG or substituted font is used. Captures are **host raster captures**,
not e-ink screenshots and not an execution of the physical renderer/HAL.
All bitmap bounds are checked. Actual app lifecycle, rules, journal and render
cases run at 75 KiB and 2 MiB, portrait and landscape. The larger instrumented
contract and synthetic-board fixture run at 2 MiB; their extra fixture closures
exceed 75 KiB. The actual entry-point constrained capture runs at 75 KiB.

`input.py` compiles unmodified extracted bodies of the pinned SDK hold classifier,
query and suppression functions, MappedInput's tap/hold mapping, production Lua
callbacks and get_device, with the real LuaTouchDispatch helper. The native fixture
supplies hardware snapshots, time and coordinate normalization. It exercises the
real production chain, not a synthetic on_touch('hold') call. Movement, one-shot,
release, cancel, disabled, multi-contact, short tap and legacy behavior are checked
at 2 MiB. A smaller production-chain hold/one-shot/no-release-dig case runs at
75 KiB; the full assertion fixture itself exceeds that cap.
`--legacy-dispatch` deliberately replaces only the helper with the previous
released-tap-only path: the flag-at-threshold assertion must fail (RED evidence).

Limits: touch controller sampling, full SDK serviceTouch, global edge navigation,
physical rotation, e-ink refresh and hardware memory remain owner acceptance tests.
The original core/storage are unchanged, with original C++ differential oracle
results recorded separately in the task evidence. Existing Solitaire device/tap
preservation tests are also run separately.
