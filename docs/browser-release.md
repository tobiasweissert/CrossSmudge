# Browser 0.2.0 owner-test release handoff

This is an integrated local preview for the owner's store. The parent performs
frozen review, commit/publication and raw URL/hash verification. This worker must
not push, publish, flash hardware, deploy a backend or send email.

Requires Sticky/X4 Pro PSRAM firmware providing API5. Install the matching
`sticky-owner-test` firmware separately, then update Browser through the owner's
App Store. An app update does not update firmware. Keep the old BIN/ELF and SD
backup available. Do not erase applications or saved data. Browser ID and icon
remain stable; B1 Jina journals remain untouched, and B5 saves use the existing
network app's isolated save namespace. No EPUB cache reset is needed.

The catalog includes every browser source dependency. A reproducible ZIP and
SHA256SUMS are prepared in the external evidence artifacts directory; target
BIN/ELF are added only after a successful real build. `HANDOFF.md` there records
exact commands, source freeze hashes, build outcomes and remaining acceptance.
No release success may be inferred from this document alone.

## Validation and hardware checks

Run `python3 test/browser/run.py` (existing ArduinoJson, JPEGDEC and PNGdec library
paths can be supplied by environment; no downloads). The productivity suite keeps
its C3 constraint tests and invokes this separate PSRAM browser suite. Host
captured articles are compatibility inputs only, never packaged as live news.

After `toolchain-ready.json` appears, use the parent-specified PlatformIO binary,
shared core directory and build lock. Build only `sticky-owner-test`, SDK
b784446302c076b4b5a622627867f0c80905cc50, platform55.03.37/core6.1.19. The shared
cache lock and previous baseline artifacts are parent-owned. Do not install or
build while provisioning is incomplete.

On a physical reTerminal Sticky, trust displayed UTC in App Settings, connect
Wi-Fi and open Browser. Follow a live feed headline and an inline article link;
check title/useful body, Back/Forward position, cancel then new address, page
turns, keyboard cursor, bookmark, find, font size, Reader/Simplified, GET search,
lazy JPEG/PNG and enlarged image. Check a midtone photo retains visible detail,
Find displays a late paragraph match, nested heading links jump correctly, and
more than 24 offline/mixed opens retain only the newest 24 history positions. Rotate and repeat touch checks. Disable app
networking and reopen a saved page offline. Interrupt a transfer and an SD write;
previous content must remain readable. Exit/reopen repeatedly and inspect logs,
internal/PSRAM free and largest blocks and task stack high-water marks. Verify
no persistent image handles or unexplained heap decline. Panel ghosting, decoder
latency, TLS SRAM reserves and physical touch remain device acceptance items.

## Known limitations

Public HTTPS, readable HTML/RSS/Atom and a bounded CSS subset only. No scripts,
React execution, credentials/cookies, paywall/CAPTCHA bypass, POST, uploads,
webfonts or desktop layout. Identity or one verified gzip member with Content-Length/chunked framing is
supported; close-delimited responses fail. Pages fit1 MiB encoded/decoded, CSS
24 KiB and images768 KiB. Text is capped64 KiB; complex
markup may hit token/depth limits. Only the first bounded external stylesheet
is fetched. Source images must be JPEG/PNG, <=768 KiB and <=2 megapixels with
2048 maximum side. Images need an explicit tap and are session-only offline.
Mode changes restart at the beginning; font/find/orientation rebuild pagination to the reading anchor.
Back/Forward positions survive within the session; reopening starts at page1.
English app UI and ASCII case folding for find. No measured Sticky free RAM,
physical acceptance or all-site compatibility is claimed by host tests.
