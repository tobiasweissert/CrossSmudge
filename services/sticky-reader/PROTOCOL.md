# SRP1 public-mode protocol (Browser 0.3.1)

This supersedes only the keyed admission/provisioning requirements in the previous
SRP1 contract. Extraction/schema/SSRF/TLS/cache/ownership/worker bounds remain.
Canonical native origin: **https://sticky.tobiasweissert.ch**, hardcoded; callers
cannot select a host, headers, credentials or arbitrary endpoint. Deployment also
retains explicit sticky-browser.tobiasweissert.ch routing and certificate SAN for
installed firmware. There is no wildcard or redirect/fallback to another origin.

## Admission and capacity

Production starts without secrets and ignores SRP_VERIFIERS. No Authorization is
required. Legacy Authorization, X-Device-ID, X-Forwarded-For and other caller
identities do not influence admission, ownership, quota or cache identity and are
never forwarded to a publisher. Exactly one `anonymous` principal/rate state is
preallocated. Explicit verifier maps are only an optional library regression mode;
production command and deployment never select that mode.

Origin is absent or exactly the canonical HTTPS origin. Existing method,
Content-Type, body (2048 UTF-8 bytes), header (8192 bytes), strict JSON and allowlist
checks remain. Public mode is not an unrestricted fetch proxy: it exposes only
seven typed POST endpoints and the existing health check.

All public clients share 60 operations/minute, 120 polls/minute, 10 new opens/minute
(with existing burst/spacing), 64 MiB fetched/day, 16 documents/principal, one
distinct active open, the global serial worker and queue of four. Global cache
remains 64 documents/64 MiB with 30-minute TTL, bounded maps/IDs/tombstones and
capacity held until owned children actually stop and are reaped. Existing pod
500m CPU/192 MiB hard memory, 64 MiB cache and 32 MiB tmp limits remain. Limits
may make one public user's activity deny service to another; headers cannot buy
more capacity.

## Endpoints and IDs

All IDs are opaque random 128-bit values encoded as 32 lowercase hexadecimal
characters; they are not passwords or user accounts. Request/response shapes are
unchanged from SRP1:

- `/v1/open`: `{v:1,rid,url,mode:"reader"|"simplified",images:"on-demand"}`.
- `/v1/poll`: `{v:1,job}`.
- `/v1/chunk`: `{v:1,doc,chunk}` (zero-based chunk).
- `/v1/locate`: `{v:1,doc,fragment}`.
- `/v1/find`: `{v:1,doc,text,after:null|{chunk,block,run,byte}}`.
- `/v1/image`: `{v:1,doc,asset,width,height}` (1..800 each).
- `/v1/cancel`: `{v:1,rid}`.

Open remains body-idempotent per RID; conflicting reuse is rejected. Cancel-before-
open tombstones remain 60 seconds. Shared ownership means a holder of a valid ID
can access/cancel its public extraction regardless of Authorization. IDs should
not be disclosed, but this is **not private storage or per-user isolation**. The
operator/hosting provider sees requested URLs/content; publishers see the server
IP. Cache expiry is not a claim about infrastructure snapshots or backups.

Pending is HTTP202 with `job,retry_ms` (500..2000); ready/chunk is HTTP200 with
versioned typed semantic blocks, opaque image assets, anchors, chunk count and
honest truncation. JSON decoded cap is 49152 bytes, 8192 decoded text bytes/chunk;
errors are closed stable codes <=2048 bytes; images are verified palette 1-bit
PNG <=65536 bytes, not original publisher media. Redirects, Cookie/Set-Cookie and
Location are rejected by the native backend lane, including empty fields/trailers.
No remote code, script execution, login/paywall bypass or NYT media is added.

## Native/app compatibility

API remains 7. New capability **browser_backend_public_v1** is true only for the
PSRAM Browser capability with the native backend/RNG adapter. Browser0.3.1 checks
that explicit flag and request-ID/request functions, defaults to Assisted when
supported, emits no Authorization and reads no private file or NVS token. Old
API7 keyed firmware lacks the flag and gets honest firmware-update guidance.
Manifest min_api stays 6 for Direct/Offline and existing caches.

`browser_backend_v1` remains advertised on new capable firmware for Browser0.3.0.
Legacy `browser_backend_configured()` returns capability and configure is a
credential-free no-op (or busy/unavailable), never reading/deleting/importing old
secrets. All HTTP worker/resource ownership, CSPRNG eligibility, cancellation,
TLS certificate/hostname/date checks and public DNS numeric pinning stay intact.
Ordinary API3/API6 HTTP header/method/redirect policy is unchanged.
