# Sticky Trivia visual parity regression

`python3 test/trivia_ui/run.py` compiles vendored int32/float32 Lua at a 2 MiB VM limit, the actual production `LuaRuntime` library setup, the actual `l_getDevice` with configured Sticky flags, and extracted production shape/bitmap raster methods. Host storage/transport/time are explicit deterministic adapters; there is no host-font substitution. `--app-dir` accepts an extracted candidate package. `--test <fixture.lua>` runs one focused fixture.

The routine entrypoint uses the shared `test/crossplay_ui_parity/build_capture.py` extractor; `--work-dir` selects an isolated evidence/build directory.

The routine entrypoint covers:

* A supplemental production draw-contract regression: original 76px black header and real licensed glyph masks. The legacy host-text recorder is structural diagnostic only, not visual approval.
* Actual touch and physical-button callbacks for correct/wrong answer selection, queued async requests, stale completions, six-second cooldown including signed rollover, error recovery, offline checked-cache replay and failed writes.
* Complete long question/answer reading through real controls in both orientations, including mixed case/Latin-1, unbroken strings and explicit unsupported-codepoint escapes. The READ control is visible; complete text is not hidden behind an unmarked clipped question.
* Production captures for Notice, Choice, wrong/correct result, and long-text last pages in both orientations. All glyph draws are checked against viewport bounds.
* Full-image pixel equality for the portrait provider Notice, plus three independently rendered original native Choice fixtures, in explicitly scoped portrait regions: header/title, question, all four option rows and their result markers, END target. These regions have zero differing pixels. This is not full-image equality: source-only difficulty/score and unsupported HIDE differ from the actual provider status/skip/reading actions, as documented by `reference/sources.json`.

Notice and Choice native references are original `TriviaScreens` builders with actual EpdFont masks, fixed-point kerning and production GfxRendererTarget at CrossPlay `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. The fixture model, renderer hashes, licenses, screenshot digests and explicit comparison rectangles are pinned in `reference/sources.json`. `repack.py` losslessly regenerates runtime assets from those immutable source objects.

Non-Sticky preservation is separately established by exact baseline main bytes in `legacy_main.lua`, unchanged provider/cache/JSON/old UI bytes, and the unchanged existing `test/network_apps/trivia_test.lua` fixture. A 75 KiB host observation of that full legacy fixture OOMs at the same progress point for both baseline and candidate; it is not reported as a C3 success.

These are host raster/input tests. On Sticky, update files in place, keep both existing question cache slots, open a cached/live question, tap each option, check correct/wrong markers, next/skip cooldown, long question READ and long answer pagination, physical selection/confirm, both viewport orientations, and END/exit/reopen. No cache reset or save deletion is required. Physical e-ink acceptance remains open.
