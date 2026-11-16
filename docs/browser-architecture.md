# Sticky Browser 0.2.0 architecture and API 5 contract

The product remains a Lua app. Native code provides HTTPS, PSRAM allocation,
font advances and existing streamed JPEG/PNG decoders. It does not execute web
scripts or turn the firmware into a desktop browser.

## Capability and compatibility

The manifest uses `browser: true`, `min_api: 5`, `requires_psram: true`, and
`network: {"domains": []}`. The empty API3-compatible domain declaration is
intentional: old hosts reach their firmware-update guidance instead of rejecting
an unknown network object. API5 only grants this capability to the `browser`
app ID, on a PSRAM board (or an explicitly constrained host simulator).
Other apps retain their existing allocator, four-domain limit, 8 KiB response,
GET/POST behavior, callback and redirect denial. API4 is not revived.

`http_request{url,timeout_ms,headers}` returns an integer request ID or nil/error;
`http_cancel(id)` signals the worker. `on_http{id,status,error,body,url,content_type}`
is delivered under the Lua mutex, never from the worker. Browser requests are
GET-only and public HTTPS. One global job slot outlives a cancelled activity
until its worker drains. Lua retries busy after cancellation; IDs and navigation
generations reject stale callbacks.

Browser requests add `resource="page"|"css"|"image"`. Completion contains an
opaque session-owned `handle`, `size` and checked-object `hash`, rather than a
whole `body`. `http_read(handle,offset,count)` accepts exact integers and at most
8192 bytes; `http_release(handle)` deterministically removes the spool.
`http_store(handle,name)` copies to a new independently named saved-data object,
checks close and rereads size/checksum before success. It never renames or
replaces an existing file. Four completed resources may be held by one session;
exit frees them, while an active cancelled worker retains its own reference.
Orphans are cleaned only after acquiring the exclusive worker slot, so a new
session cannot delete a draining writer's object.

Pages are limited to1 MiB, CSS24 KiB and images768 KiB, both encoded and decoded.
Native socket bytes stream into an immutable SD spool. HTML is validated and
tokenized through a single8 KiB cached source adapter, with repeated bounded
passes rather than a response-sized Lua string. CSS alone materializes at most
24 KiB for rule parsing. Images are verified/copied natively without a Lua body.
Dispatch/read OOM is recoverable; outer native owners close files even when Lua
longjmps. Candidate read, parse and publication remain protected before swap.

Up to four redirects are reparsed, each reconnecting through numeric-vetted
public IPv4 DNS and TLS hostname/SNI, certificate bundle, chain and explicit
date checks. Verified UTC uses the existing user-trusted monotonic anchor.
Credentials, cookies, IP literals, private/reserved IPv4 results, HTTP downgrade
and ambiguous URLs are rejected. Relative redirects are supported. Each request
has a total deadline; response headers and trailers have separate4 KiB caps.
Chunk control lines are individually bounded; at most limit+1 chunks and
6*entity-limit+4096 control bytes admit one-byte segmentation without sharing
the response-header counter. Content-Length
and chunked bodies are supported, including bounded extensions/trailers.
Browser requests advertise gzip and identity. A single gzip member is decoded
with the existing pinned miniz engine, admitted PSRAM-only state/ring (under48 KiB)
and no framebuffer loan/internal fallback. The gzip header is capped1 KiB;
optional header CRC, payload CRC32, ISIZE, truncation and trailing bytes are
validated. Concatenated members are rejected. Encoded and decoded limits and
the total deadline apply throughout. Close-delimited responses fail explicitly. No proxy or assisted-text service is called.

## Memory and drawing

Browser Lua alone uses explicit `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` realloc,
with a 4 MiB ceiling, 512 KiB PSRAM reserve and 16 KiB internal emergency floor.
There is no internal-RAM allocation fallback. TLS separately requires 112 KiB
free internal SRAM and a 40 KiB block, then retains its 80 KiB allocation budget.
The internal floor deliberately stays below TLS's temporary consumption; PSRAM
UI allocation must not fail solely because TLS uses its admitted SRAM.
These are admission policies, not measured Sticky free-memory claims.

`browser_path(name)` returns a confined saved-data path (`name=''` gives its
root). `browser_advance(text,font,style)` uses the production font advance
function, including whitespace; API3's bounding-box `text_width` is unchanged.
`browser_image(path,x,y,w,h)` accepts exact integer coordinates, confines the
path, sniffs PNG/JPEG magic, caps compressed bytes at 768 KiB, source dimensions
at 2048x2048 and two megapixels, and output to the current oriented screen.
It opts into one-bit Bayer dithering (black/white levels 0/3) in the pinned
JPEG/PNG converters, preserving luminance in BW mode. The RenderConfig opt-in
defaults to false; EPUB and other converter callers retain their four-level
behavior and DirectPixelWriter is unchanged. No new pixel buffer is allocated.
It renders directly into the single
framebuffer. There are no Lua pixel tables or full-size RGBA buffers. Factory
ownership is serialized by the activity render mutex; no handles survive draw.
Images reserve geometry before explicit lazy download; another tap enlarges.
Downloaded image objects have unique names, are checked after writes, and are
cleared on successful page replacement and exit. They are not durable offline
image caches.

## HTML, reader and CSS envelope

The quote-aware streaming tokenizer handles comments, CDATA, raw script/style,
attributes, self-closing and malformed tags. It bounds tokens (16000), nesting
(64), retained blocks (600), spans (2400) and text (64 KiB). Oversized attribute
tags are scanned without retaining their payload. Invalid UTF-8 is rejected;
named common and numeric entities decode to UTF-8. URL resolution handles
relative paths, dot segments, base, query and local fragments with HTTPS limits.

Semantic headings, paragraphs, lists, links, quotes, code/pre, images and GET
forms become compact blocks/spans. Article wins over main when it contains useful
text; main and readable blocks provide the fallback; fallback readable blocks suppress scripts, styles, navigation, footers,
asides and common cookie/advert/social/icon-font containers. This is a bounded
heuristic, not a readability guarantee. Title, author/date meta and source are
retained; RSS/Atom entries include supplied dates and link to actual articles.
Simplified mode retains navigation/forms. No headlines or freshness are invented.
The user can add feed URLs; default BBC, Guardian and Tagesschau feeds are links,
not fixtures bundled as live responses.

CSS supports tag/class/ID and up to four descendant selector parts; specificity,
source order, inline declarations and `!important`; inherited font weight/size
and preformatted text; heading defaults, clamped spacing, borders, background
dither, display:none/visibility:hidden and simple inline/block decisions.
Tables degrade to separated cells/rows. Inline/style blocks plus the first
external stylesheet are supported (24 KiB total retained CSS,128 rules and
50000 selector comparisons per document). Exhausting matching work retains
semantic/inline defaults and displays a basic-style notice. Unsupported at-rule
bodies, including print/media rules, are skipped as balanced blocks. Checked CSS sidecars
preserve the same context offline. No media queries, pseudo selectors, flex,
grid, positioning, webfonts or full CSS layout. Font sizes map to three installed
font contexts; bold wrapping/drawing/hit targets use the same context.
Reader/Simplified switches restart at the document beginning. Font/find and orientation reflow rebuild bounded pagination up to the semantic
anchor. History retains the page index and anchor list, so Previous reaches
earlier content after Back. Start of document is always available.

Page-first chrome, 44-pixel rows/controls, nonoverlapping inline link hit targets and lazy
pagination share portrait/landscape bounds. Back/Forward retain semantic reading
position in session. The keyboard has a UTF-8-safe cursor, a cursor-following viewport, delete and URL symbols.
Fragment IDs (at most 256, each at most 256 bytes) map to semantic block/span/UTF-8
anchors, including empty wrappers and nested inline headings; Reader filtering
and metadata insertion reindex the retained anchors. Find jumps to the matching
UTF-8 byte position, including matches late in a single span.
Find is case-insensitive for ASCII; GET text/search/hidden inputs are URL encoded
and require explicit submit. There is no POST, login, select or file upload UI.
UI strings in this Lua preview are English; native new error strings use I18n.
Redraw requests are event-driven and coalesced by the host activity.

## Persistence and evidence boundaries

A new B5 namespace leaves old B1 Jina journals untouched and ignored. Immutable
independently named page objects are read back by bounded chunks/checksum before
publishing a two-slot checked index. Candidate lists stay local through commit,
including OOM. There is no rename or ambiguous source deletion. Cleanup protects
objects named by either valid index: eight current and at most eight previous
page objects, plus interrupted orphans reclaimed on the next cleanup. Bookmarks
(20), history (24 in session through one network/offline push helper, including
remembered pagination tables), saved pages (8), feeds (8) and images (32/page)
are bounded. Position across process restart currently restarts at the beginning.

`python3 test/browser/run.py` is the routine native/Lua regression entrypoint.
Host Lua uses the actual vendored int32/float32 configuration and library startup;
API bindings/parser/layout are production code. Host image tests use the pinned
codecs and production app helper/converters with mock storage/display and Sticky
memory macros. Font captures use production font metrics/bitmaps through a host
raster adapter, not the physical display. Target compile, host source evidence
and physical Sticky acceptance remain distinct gates. Private live captures and
raw RED/GREEN logs live outside Git in the task evidence directory.

Sanitizer scope: all owned native/Lua code is checked fully. The pinned JPEGDEC
x86 decoder and PNGdec inflate/inffast objects deliberately use unaligned word
accesses; only alignment checking is disabled for those vendor objects. Their
address/other undefined-behavior checks remain enabled. PNG uses its actual
pinned C inflate code in host tests. LSan is unavailable under the host ptrace
sandbox; bounded Lua accounting asserts zero retained allocations on shutdown.
