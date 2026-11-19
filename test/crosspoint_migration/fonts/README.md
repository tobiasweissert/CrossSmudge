# Bounded font-core/bitmap migration tests (not full SDK)

Run from this directory with `make test` and `make sanitize`, or from the repository root:

```
python3 -m unittest discover -s test/crosspoint_migration/fonts -v
FONT_SANITIZE=1 python3 -m unittest discover -s test/crosspoint_migration/fonts -v
```

Needs installed Python, g++ and Ruby; installs/downloads nothing. Compile/execute artifacts use task TMPDIR and are deleted after each case. The asset check is syntax-only (no object file). There is no GTest/PlatformIO/SDK dependency.

Coverage: actual production EpdFont/Family + Utf8 geometry, point/legacy construction, shared fallback glyph source, dynamic kerning field, solid metrics, owner/upstream generated IDs, complete owner+upstream bitmap headers; actual Bitmap.cpp over an in-memory file adapter checks V4 palette offset, display-order luminance, packed rows, bounds, no-dither resize refusal and production dither/gray-plane helpers. Normal and sanitizer runs use the same methods.

This does NOT compile GfxRenderer, SdCardFontSystem, the SDK or TtfEpdFont, and does not render fonts through FreeType. It is not pixel/UI/device acceptance. Remaining font/renderer conflicts must be resolved before target compilation.
