# Hacker News original UI regression

Run `python3 test/hackernews_original_ui/run.py`. `--app-dir` qualifies an extracted SD candidate; `--work-dir` selects isolated evidence; `--test <name.lua>` runs a focused fixture.

The routine entrypoint uses the shared production extractor, actual configured Sticky identity, vendored int32/float32 VM at 2 MiB, and actual `LuaRuntime` essential/network library setup. It renders original licensed glyph/sprite bytes through extracted production one-bit primitives, exports full PNG/trace/state artifacts, and fails on unsupported drawing methods. Storage, transport and clock alone are fixtures.

It tests actual callbacks for original chrome, long-title ellipsis, all four feeds, six controller items at the original row size in both orientations, correct physical row activation, source empty-thread text, feed-menu focus, article/comment swapping, error placement, close/reload, 64 KiB newline/word/normal/UTF-8 article tails, and the 128 KiB comment limit with duplicate active/pending More input. No controller-state setters replace input tests.

Pinned native references are original `HackerNewsScreens` builders with actual SDK typography, EpdFont masks, ink measurements, fixed-point kerning and source wrapping. Busy and ordinary Reader are compared over full images in both orientations. List comparison excludes only its footer-label text band: source FRONT PAGE/SAVED maps to actual current feed/FEEDS because the port has four feeds and no Saved library. All other pixels remain checked. Model fields, file hashes and scope are in `reference/sources.json`.

The entrypoint also compares full legacy startup/error pixels for X3, X4, X4 Pro, simulator, unknown and missing identity fixtures in both orientations (12 before/after pairs). These are identity fixtures and do not claim physical memory acceptance.

It runs the three existing provider/accepted repair fixtures. Only their host `io.open` asset-read adapter is substituted with the bounded native fixture read; their assertions stay unchanged. Their missing-device-API path exercises the exact preserved legacy presentation. The new Sticky 64 KiB/128 KiB tests independently exercise the repaired controller under real original-font rendering.

Hardware: on Sticky, merge the app files in place, retain all feed/item/article cache slots, check cold/cached startup, feed switching, every landscape slice and physical Confirm, direct title/count taps, article/comments and parent Back, long-page endings, More comments, cancellation, failed writes and reopening. No cache reset is needed. Host pixels, production ABI availability and physical e-ink acceptance remain separate.
