# CrossPlay network app ports on frozen APIv3

Worktree base: `da264a5844e87abb4c694e4143b2972f435f99c8`, branch
`feat/lua-network-apps`. Intended stacked PR: `feat/lua-network-apps` →
`feat/lua-http-api` in `tobiasweissert/CrossSmudge`. Parent owns commit/push/draft
PR creation. No host/API/firmware edits, firmware builds, flash, publishing,
catalog edits, or changes to the other writers' worktrees are part of this work.

## Delivered and remaining scope

These are real-request Lua implementations, not fixtures advertised as live.
Native tests use explicitly synthetic input; public network operation remains
unverified because the sandbox denies sockets. Neither app is release-ready.

| Target | Actual implementation / original source | Status and smallest remaining requirement |
| --- | --- | --- |
| Hacker News | `apps/cp-hackernews`; official Firebase top feed + individual items, explicit Jina article reading. CrossPlay `HackerNewsActivity.cpp:28–44` uses Algolia + Jina. | Implemented bounded preview. Changed provider for feed avoids large Algolia hit/comment payloads. Full articles over 8 KiB require bounded streamed-to-file responses and paged file reads, or a separately reviewed provider-side page service. No such extension is called. Comments/shelf parity not implemented. |
| Connections | CrossPlay `ConnectionsActivity.cpp:45–47` downloads `https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json`; `ConnectionsImport.cpp:87–142` consumes dated records with `answers[].group`, `members`, `level`. | **Not implemented.** This is a whole multi-year archive, not a per-date endpoint. APIv3 cannot stream it, Range is not allowed, and GitHub raw has no JSON query projection. Need a bounded stream/file API + incremental dated-record importer, or an authorized maintained per-date feed ≤8 KiB. Source has no visible license grant; verify remote usage/redistribution permission first. Do not infer puzzle rights from CrossPlay's MIT code. No copied puzzles or dummy manifest/package. |
| Trivia | `apps/cp-trivia`; Open Trivia DB `amount=1&type=multiple&encode=url3986`. Original `TriviaActivity.cpp:33–39` downloads CrossPlay GitHub release pack/manifest. | Implemented **provider variant**, not original pack parity. OTD documents unauthenticated API use and CC BY-SA 4.0 content. One bounded question, validation, answer feedback, explicit next/skip, six-second cooldown, checked replay. Original pack needs redirect-policy review, streamed download, and pack reader; no fake pack compatibility claimed. |
| Study | CrossPlay `StudySync.cpp:109,161,432,478,522,591`, `StudyFsrs.cpp`, `StudyScheduler.cpp`, `StudyDeck.cpp`. | **Not implemented.** Actual `sync.ma-r-s.com` bearer-auth pairing/deck/sync plus streamed files and complete scheduler are needed. Minimum host addition is a separately consented credential handle with domain-scoped Authorization (never exposed to app files/logs), bounded file transfer/commit; full FSRS/deck/media/sync conflict logic still needs porting and prepared real deck fixtures. HTTPS alone does not implement Anki compatibility. |
| xkcd | CrossPlay `XkcdActivity.cpp:778,878,1135`; release image pack plus `https://xkcd.com/<num>/info.0.json`. | **Not implemented.** Metadata alone is not a comic viewer. Need bounded image download/conversion to renderer-compatible 1-bit assets, or authorized prepared raw-image pages and streamed file storage. Existing Lua raw drawing is not a PNG decoder. GitHub release redirects also exceed frozen policy. |
| Instapaper | CrossPlay `InstapaperSync.cpp:42,63,118,139,238–243`, host `read.ma-r-s.com`. | **Not implemented.** Original pairing/poll/sync/status/article protocol uses bearer authentication and streamed article files. Same narrowly scoped credential-handle + bounded transfer extension as Study, then actual pairing/revocation/token lifetime and article/index tests. No arbitrary key endpoint, public token query, or token cache substitute. |

Connections provider describes the archive and its `level=-1` change after
2025-09-20: https://github.com/Eyefyre/NYT-Connections-Answers. No exact current
archive byte count was captured here; the external capture records it. No
Connections gameplay is claimed tested. Source review alone does not establish
remote terms permission.

Sources consulted 2026-11-15:
- HN public API: https://github.com/HackerNews/API
- Jina Reader / optional API keys: https://jina.ai/reader/
- Trivia API, rate limits, encoding and license: https://opentdb.com/api_config.php
- CrossPlay source was read from `/root/github/crossplay/src/apps_local/`, read-only.

## Implemented constraints and verification

Both packages declare only their actual exact domains and `min_api: 3`. No
credentials, invented APIs, POSTs, redirects, automatic retries or fallback
snapshots. The production bridge validates their submitted URLs/options against
those exact domain grants. Next requests are submitted from `on_update`, never
inside `on_http`. IDs bind responses to current work, including item identity.
Exit cancels, clears queued work/data, and ignores subsequent callbacks.

JSON decoding is bounded to 8192 bytes, 1024 values, depth 12, strict UTF-8 and
surrogate pairs, duplicate-key rejection, finite numbers and exact int32 integer
bounds. Feed/item/question schemas are checked separately. UI measures glyphs,
wraps long URLs, pages within current bounds, bounds menu previews, and draws
explicit touch footer labels (firmware hides its own in landscape). Only one
page's text layout is held; no framebuffer allocation is added.

Cache uses fixed app-relative names; every save writes the alternate slot,
checks length + metadata/body checksum, then reads back exact bytes before
accepting it. Corruption is rejected on reopen. No host rename/atomic-write API
is assumed. This is interruption resilience, not malicious-SD authentication or
physical power-loss durability proof. The cache body ceiling is 8203 bytes to
hold an entire 8192-byte article plus int32 ID/newline. Network ceiling stays
8192. Local device timestamps can be wrong if the user's clock is wrong.

`bash test/network_apps/run.sh` builds only native test programs from shipped
Lua C sources (int32/float32); no PlatformIO/firmware build. It tests production
Lua parsers, lifecycle, touch, pagination, errors, cache corruption/interrupted
writes, stale completions, cooldown and replay. A separate C++ harness links
**production `LuaHttp.cpp` and `HttpCore.cpp`**, with synthetic native transport
and UI/file doubles; it exercises real registration/dispatch and path isolation.
This does not simulate Wi-Fi, TLS, DNS, hardware rendering, SD or a public server.

Lua allocations are capped at 2 MiB in native tests and return to zero on close.
Large-data HN tests include 500 IDs, a 1024-byte title, 6000-byte story text, and a
full 8192-byte article plus checked cache readback. Trivia stress uses long
question/answers. The 75 KiB C3 allocator passes tiny fixtures but **fails large
HN data**. No C3 support claim: packages are local S3 previews, absent from the
App Store. Native Lua peaks exclude firmware UI, Wi-Fi, TLS and allocator
fragmentation. Small passing examples do not establish hardware fit.

Frozen host also requires Wi-Fi before entering network apps. To allow cache
viewing after canceling that picker, parent must separately review an offline
entry path with network grants unchanged and explicit offline request failure.
The apps do not silently change this firmware behavior.

## Exact fixture preparation outside sandbox

Run on the parent's network-enabled host, from this worktree:

```sh
python3 scripts/network_apps/capture_fixtures.py /root/github/.host-data/crosssmudge/lua-network-apps/public-fixtures-20261115
bash test/network_apps/run.sh
CP_FIXTURES=/root/github/.host-data/crosssmudge/lua-network-apps/public-fixtures-20261115 \
  /root/.hermes/cache/scratch/cp-network-tests/lua test/network_apps/provider_fixtures.lua
```

Choose a new output directory for another capture: script refuses to overwrite.
It fetches exact allowlisted public URLs (including the first actual feed ID),
uses system CA roots, no credentials, no redirects, records raw body bytes,
headers, UTC retrieval time, SHA-256 and CA paths. It fetches Jina's anonymous
`https://example.com/` sample only, not arbitrary feed article URLs or paid calls.
Trivia uses one call. It also measures the original Connections archive and
xkcd metadata. Errors and provider refusals remain visible; do not manufacture
successful fixtures. Treat `candidate_for_v3` as preliminary only: native
parser/header/chunk limits and actual ESP TLS still need testing. Keep captures
outside package directories and review content rights before committing them.

The sandbox attempt is recorded at
`/root/github/.host-data/crosssmudge/lua-network-apps/sandbox-capture-attempt/provenance.json`;
all requests failed with `Operation not permitted`. No raw live bodies exist
from this run. This is not a provider outage or successful endpoint validation.

For exact framing replay, parent may use the existing PR1
`test/lua_http/openssl_probe.cpp` harness with its loopback certificate fixture
procedure; this task did not alter it or claim new loopback/ESP TLS results.

## Local packages and hardware check

```sh
python3 scripts/network_apps/package.py /root/github/.host-data/crosssmudge/lua-network-apps/packages
```

The deterministic stored ZIPs contain one `cp-*` directory each. Extract into
SD `/.smudge/applications/`. Packaging compares every extracted ZIP member byte
with source and emits sizes/SHA-256/file inventories. No catalog is edited,
no existing version bumped, and nothing is pushed or published. Preview
manifest versions apply only to these new app IDs.

On **Sticky or X4 Pro**, sync time, independently verify UTC, trust it in App
Settings, launch, approve listed domains, and connect Wi-Fi. HN: Refresh, check
real feed title and next/previous story order, open story, explicitly select
linked article; small compatible articles should paginate, oversized ones
must show `body_limit` with previous cache intact. Trivia: fetch, choose/read/
answer, next, then repeated next within six seconds; wait/retry must be explicit.
Test portrait/landscape touch and buttons, untrusted time, Internet loss,
cancel during DNS/read, Home/sleep/reopen, corrupt newest cache slot and SD
write failure. Confirm old valid cache recovery and no post-exit update.
Inspect internal heap/largest block, PSRAM, TLS allocation retention and worker
stack watermarks across repeated sessions. **No EPUB cache reset is needed.**
Do not flash or build firmware as part of this task; use parent's existing APIv3
hardware image only when the parent performs this verification.

## Concrete extension requests for parent review (not implemented calls)

1. **Archive/article transfer:** a separately versioned asynchronous download
   operation with the existing domain grant, public-DNS/TLS policy and single
   job slot; fixed app-save-relative destination, explicit `max_bytes` (suggest
   4 MiB ceiling), cancellation, and a committed-file result only after complete
   framing validation. Native storage owns a temporary file and commits it on
   success; Lua gets no partial-success payload. Keep scratch buffers bounded
   (≤8 KiB), reject compression/redirects as today, remove only the operation's
   own temporary file on failure. Existing `read_file(path,len,offset)` can then
   feed a ≤8 KiB incremental Connections record parser without loading the
   archive. Test interruption, cap overflow, cache recovery, save confinement,
   cancellation drainage and C3 internal RAM before exposing it. An 8 KiB
   response-cap increase alone is not an acceptable fix.
2. **Provider credentials:** a domain-bound opaque credential handle, explicit
   pairing/approval/revoke UX and host-owned storage. Attach only that handle's
   bearer header to its approved host. Never return tokens through Lua events,
   cache them on SD, put them in URLs, or enable arbitrary Authorization headers
   globally. Real Study/Instapaper bridge interoperability and token exchange
   need provider agreement and fixture access; this is a design request, not a
   fabricated implemented endpoint. No POST retry on connection failure.
3. **Comic pipeline:** bounded conversion of downloaded PNG/JPEG into validated
   1-bit raw strips with declared dimensions/byte count, or an approved external
   prepared-asset service. Reuse existing raw drawing only after validation;
   image metadata/title alone is not xkcd parity. Image rights and any provider
   service are separate from permission to execute CrossPlay's MIT code.

These requests are intentionally not patches against the frozen HTTP commit.
Parent owns any API refinement/rebase and must approve its exact scope first.
