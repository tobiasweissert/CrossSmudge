# Wavelength — local Lua adaptation

Implemented: All 245 original spectrum pairs; nonrepeat chosen pairs with one/two choices; private target across all 20 slots; co-op and team end-call scoring; zero-point practice round; session total and saved state.

Limitations: Lifetime histogram/statistics are absent because the current core does not store them. Original artwork is present on the enhanced host path below; compact hosts retain the list frontend. Team mode keeps the original shared session scoring formula, not invented separate team totals. Hidden target reopens behind a reveal gate. English only.

Persistence uses app-relative checked two-slot journals, semantic input validation and readback; failed writes leave the previous slot available. These saves are separate from original CrossPlay and existing CrossSmudge IDs. No firmware or network hooks are installed.

Validation: `test/crossplay_ports/additions_run.py` uses the exact stock int32/float32 Lua VM at 76,800 and 2,097,152 bytes, both orientations, rule fixtures, callback/render instrumentation and journal failures. See repository `docs/crossplay-ports.md`. These are native adapter results, not e-ink screenshots or hardware measurements.

Hardware verification on Sticky v1.6.5.3: Pick a spectrum privately, reveal and hide target, guess, exercise both end-call directions, verify zero practice points and exact/adjacent scores, exhaust deck and reopen. Check serial logs for OOM/Application Error, internal free/largest heap and PSRAM. Repeat touch and buttons in both orientations. No EPUB cache reset is needed.

## Original-screen presentation (PSRAM + release callback)

The enhanced path ports the original `WavelengthScreens.cpp` material from reference commit `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`: original Jersey cuts, pass/look icon masks, two-spectrum cards, private hold-to-see screen, clue page, vertical twenty-rung ladder and large guess, side-call screens, target/guess reveal, scoring table and session summary. Tapping a scale position selects its original slot; side buttons step one. At the extreme slots the impossible side-call direction is not offered. The first round still scores zero.

Hold-to-see is enabled only when `smudge.has_feature('touch_release')` is true. An older APIv3 host, or a host below 512 KiB, keeps the compact controller with its explicit hide control. A supported touch hold reveals after the runtime's 500 ms threshold; the release callback hides and requests a full refresh to clear the target ghost. Confirm may also be held to peek. Core/data/save modules are unchanged, and returning through the menu does not discard an in-flight round.

Known gaps: the current core has no original lifetime distribution or boot/session-age marker. The enhanced front door therefore shows genuine saved session totals/last-round points, not invented historical statistics; the original lifetime histogram and stale-evening question remain unported. Lua text measurement/fitting differs from FreeInkUI, and native captures are not pixel-parity or physical e-ink evidence.

Run `python3 test/party_ui/run.py` and `python3 test/party_ui/capture.py`. Asset hashes and original licenses are included. On Sticky, verify the target stays hidden before a hold, disappears on lift/drag/cancellation/Back/Home, and leaves no readable ghost. Exercise both side calls, slots 1/20, practice/normal scores, pause/menu/resume and exit/reopen. No EPUB cache reset is required.
