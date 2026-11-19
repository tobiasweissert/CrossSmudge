# Browser 0.3.1 public keyless assisted reader

Browser keeps its API6 Direct and Offline paths. Assisted requires API7,
PSRAM and the explicit `browser_backend_public_v1` capability. API7 alone or the
older `browser_backend_v1` flag is insufficient: old keyed firmware shows an
update-needed state, while Direct/Offline remain available with manifest min_api=6.
The firmware owns the fixed HTTPS origin
`https://sticky.tobiasweissert.ch`; Lua cannot supply an origin,
Authorization header or credential. Ordinary Browser HTTP remains GET-only.
All backend redirects are rejected, including redirects to that same origin.
Existing TLS hostname, certificate, date and public-DNS pinning checks remain.

Connect Wi-Fi through the existing firmware flow before Assisted navigation.
The request-ID helper requires connected Wi-Fi so the ESP hardware RNG has
fresh radio entropy. Public mode does not change the global network opt-out.
A capable installation defaults to Assisted without any token or private file,
but restoring an offline page never submits its address to the server.
Errors preserve the readable view and offer an explicit Retry or mode choice.
There is no automatic Direct or extraction-mode fallback.

Use Browser's menu to select Assisted, Direct or Offline. There is no import or
configuration action. Native requests emit no Authorization; no SD private file
or NVS credential is read, imported or deleted. Legacy API7 configured/configure
bindings are credential-free capability/no-op shims for Browser0.3.0, never
provisioning. Ordinary HTTP Authorization and Browser POST remain forbidden.

The public service uses one bounded anonymous principal and shared capacity.
Legacy Authorization and invented device/proxy headers do not create identities
or extra budgets, and are never forwarded to publishers. Opaque random request,
job, document and asset IDs remain required, but are not account isolation:
someone holding an ID can access/cancel its public extraction. This is not
private storage. Cache lifetime is bounded; anyone can exhaust the shared quota.
The backend and hosting operator see addresses/content, publishers see the
backend IP, and SD caches remain plaintext. HTTPS protects transport only.
The old sticky-browser.tobiasweissert.ch route remains explicitly configured
temporarily for installed firmware; new firmware never selects it or redirects.

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
public-mode firmware: Apps → Browser (Assisted defaults), then
open an original synthetic article. Check title/byline/tail, chunk crossing and
Back/Forward, Find/fragment jumps, GET forms, image load, font changes and both
orientations. Save all chunks, disconnect networking, reopen Offline and verify
coverage plus previously loaded images. Interrupt a save/request/image, navigate
again after drain, and verify late responses never replace the view. Verify
fresh setup with no private file/NVS credential. Exercise AirDrop URL
handoff during asynchronous startup: ACK follows admitted ordinary navigation,
not fetch success; failed ACK/startup/exit retains Retry/Cancel. On API6, confirm
Assisted is unavailable while Direct and existing B5/B1 saves still work. On C3,
the Browser capability remains unavailable and ordinary apps retain their HTTP
limits. No EPUB cache reset is needed; preserve existing Browser caches during
these checks.

The source ZIP is a parent integration artifact. It does not update the App Store
catalog or deploy a server/firmware. Full firmware linking, actual backend E2E,
trusted deployed TLS, independent review and
physical Sticky checks remain parent gates.
