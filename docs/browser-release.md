# Browser 0.2.1 owner-test release handoff

This is an integrated local preview for the owner's store. The parent performs
frozen review, commit/publication and raw URL/hash verification. This worker must
not push, publish, flash hardware, deploy a backend or send email.

Requires Sticky/X4 Pro PSRAM firmware providing API6. Install the matching
`sticky-owner-test` firmware via Settings -> SD Card Firmware Update, then update Browser through the owner's
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
paths can be supplied by environment; no downloads). Run the main app gates with
`python3 test/integration_owner/run.py --output <owned-evidence>` as well. Host
captured articles are compatibility inputs only, never packaged as live news.

After `toolchain-ready.json` appears, use the parent-specified PlatformIO binary,
shared core directory and build lock. Build only `sticky-owner-test`, SDK
b784446302c076b4b5a622627867f0c80905cc50, platform55.03.37/core6.1.19. The shared
cache lock and previous baseline artifacts are parent-owned. Do not install or
build while provisioning is incomplete.

On a physical reTerminal Sticky, connect Wi-Fi and open Browser. HTTPS obtains
UTC automatically through main's synchronized anchor; certificate date checks remain enabled. Follow a live feed headline and an inline article link;
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

For this repair, use the same captured 20min homepage and portfolio on actual
Sticky. Record Fetching/Reading/Preparing/Images durations and the last nonsecret
TLS stage if Google fails. Press mapped Back and the global Home gesture during
HTML validation, long tags/scripts, cache copy, offline startup, Find/font/rotation
reflow, and large PNG/JPEG header/decode work. Back should retain the previous
readable page; Home should exit without waiting for parsing or image completion.
Cancel just before publication, reopen offline, and confirm the old checked page
survives. Repeat exit/reentry while native work drains; new navigation must wait
honestly instead of reusing the worker slot. Inspect internal/PSRAM largest blocks,
task high-water marks, file/worker counts, and actual panel/input acknowledgment
latencies. Do not clear B5 saves to hide a recovery failure. No EPUB cache reset is
needed. Test with a healthy SD card, then document slow/error-card behavior; host
quanta do not establish a physical Sticky guarantee.

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

Round 1 addresses LUA-1–3 only: completion waits behind ordinary UI work; CSS
read errors release the entire operation immediately. A failed index write or
readback is explicitly uncertain: the prior visible page stays, while reopen
may recover the new checked candidate. Both roots/objects remain protected and
no further publication is attempted until a healthy reopen resolves root reads.
Round 2 preserves this gate across reopen when an existing root is still
unreadable, and when cleanup first discovers the failure. Startup reports a
read-only cache. With continued injected read failures, verify zero cache writes
or deletes through page/bookmark/feed saves; restore reads and reopen to recover
the original checked candidate and its OLD fallback, including CSS. Readable
checksum-invalid roots still allow valid fallback recovery. These are checks of
`read_file`/`file_exists` results, not physical fault-detection guarantees. Test
write faults before/after mutation, readback failure, reopen and cancellation
before publication on Sticky without clearing B5 saves. Publication stays on
hold for independent re-review and the separate main-integration security/UI
composition; any composition needs a new delta review and matching build.
