# Browser 0.3.0 assisted reader

Browser keeps its API6 Direct and Offline paths. Assisted requires API7,
PSRAM, the `browser_backend_v1` capability and explicitly imported native
configuration. The firmware owns the fixed HTTPS origin
`https://sticky-browser.tobiasweissert.ch`; Lua cannot supply an origin,
Authorization header or credential. Ordinary Browser HTTP remains GET-only.
All backend redirects are rejected, including redirects to that same origin.
Existing TLS hostname, certificate, date and public-DNS pinning checks remain.

Connect Wi-Fi through the existing firmware flow before Assisted navigation.
The request-ID helper requires connected Wi-Fi so the ESP hardware RNG has
fresh radio entropy. Importing configuration does not enable networking or
change the global network opt-out. A configured installation may start in
Assisted, but restoring an offline page never submits its address to the server.
Errors preserve the readable view and offer an explicit Retry or mode choice.
There is no automatic Direct or extraction-mode fallback.

Use Browser's menu to select Assisted, Direct or Offline. Import server
configuration is a separate explicit action. The native importer reads only
`/.crosssmudge/private/browser-backend.json`, at most 512 bytes, with exactly:

```json
{"v":1,"origin":"https://sticky-browser.tobiasweissert.ch","token":"<64 lowercase hexadecimal characters>"}
```

The parent/operator provisions the real device credential privately and puts
only its verifier in the backend's app Secret. Retrieve the configuration
through the owner's private SSH/SFTP channel, not chat, a public download URL
or Git. The importer checks the exact origin and token format, stages a dedicated
NVS blob, verifies it, then verifies the active-slot selector. Failed imports
retain the prior active credential; configuration changes are refused while
the HTTP worker still owns any request. The SD file is not automatically deleted.
Lua receives only a boolean and stable error code, never the credential.

SD configuration and normal Preferences/NVS storage have physical extraction
risk. This implementation makes no encryption claim. Cached pages and images
also remain plaintext on SD. The backend operator and hosting provider can see
requested addresses/content; publishers see the backend's address. HTTPS
protects transport, not compromised storage or server administration.

SRP1 transports semantic chunks, not screen pages. The footer reports `P<local
page> C<chunk>/<chunks>`. Layout, font changes, rotation, fragment resolution
and Find operate on the current bounded chunk. A position in a later chunk
loads that specific chunk. No complete article is retained or laid out. History
retains 24 entries with semantic positions and cache references; SRP history does
not retain every screen anchor. A legal newline-heavy chunk can exceed 512 local
screens; position/reflow remains bounded by its 8 KiB text budget rather than
silently stopping at screen 512. Document details exposes the full title,
chunk coverage, extraction mode, language and truncation reason. Direction is
metadata; this change does not add RTL shaping or new font coverage.

Links use the selected transport. Forms are typed GET forms with editable
text/search fields and retained hidden fields. Find is ASCII case insensitive
and otherwise exact UTF-8. Offline Find examines only checked, available chunks
and labels that limited coverage. Missing history/cache content requires an
explicit fetch choice. Start of document returns to chunk zero.

B7 is separate from legacy B5/B1 HTML caches. Two checked roots reference
immutable wrapped JSON chunks and explicitly loaded PNGs. Roots contain document
identity, mode, chunk count, per-chunk object names/lengths and image checksums.
Object lengths let quota/GC inspect checked roots without rereading the complete
article. Publication prepares layout before the root and view commit together.
Unreadable or malformed roots latch read-only publication/GC until a healthy
reopen; old objects remain protected by both roots. Checksums detect accidental
corruption, not malicious SD modification. Limits are eight document entries,
64 chunks/document and 16 MiB of persistent B7 objects across both roots;
native spools and decode scratch are separate bounded transient resources.

The first chunk is checked before display. Save offline fetches remaining chunks
sequentially and reports complete only after all checked chunks are available.
Cancel leaves an honest partial save. Images remain on demand even in a complete
text save. An opaque asset ID goes only to `/v1/image`; no media-origin URL or
Direct image fallback reaches Lua. The client checks the palette PNG envelope,
CRC, bounds and absence of ancillary/animation chunks. The existing asynchronous
PNG codec produces its normal 2-bit PXC cache for blitting. A persistent B7 PNG
reference is published only after successful native decode; startup preserves
referenced B7 PNGs. Loading/error placeholders retain alt text.

HTTP callbacks only enqueue completion events. Poll, image and cancellation
requests begin in update after native ownership drains. Cancellation invalidates
stale generations and releases resources; a best-effort server cancellation must
not overwrite the visible navigation error. App exit cannot guarantee delivery
of a server cancel, so backend deadlines remain necessary. No hard real-time
cancellation or physical-device speed claim is made.

For hardware verification, use a Sticky with the parent's integrated API7
firmware: Apps → Browser → Import server configuration, then select Assisted and
open an original synthetic article. Check title/byline/tail, chunk crossing and
Back/Forward, Find/fragment jumps, GET forms, image load, font changes and both
orientations. Save all chunks, disconnect networking, reopen Offline and verify
coverage plus previously loaded images. Interrupt a save/request/image, navigate
again after drain, and verify late responses never replace the view. Retry a bad
configuration while keeping the old working credential. Exercise AirDrop URL
handoff during asynchronous startup: ACK follows admitted ordinary navigation,
not fetch success; failed ACK/startup/exit retains Retry/Cancel. On API6, confirm
Assisted is unavailable while Direct and existing B5/B1 saves still work. On C3,
the Browser capability remains unavailable and ordinary apps retain their HTTP
limits. No EPUB cache reset is needed; preserve existing Browser caches during
these checks.

The source ZIP is a parent integration artifact. It does not update the App Store
catalog or deploy a server/firmware. Full firmware linking, actual backend E2E,
trusted deployed TLS, real credential provisioning, independent review and
physical Sticky checks remain parent gates.
