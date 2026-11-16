# Sticky Reader SRP1 service

Private, pure-Go conversion service for the fixed origin `https://sticky-browser.tobiasweissert.ch`.
It implements all seven POST endpoints in the parent-frozen SRP1 contract. The application owns
HTTPS fetching; a single owned subprocess performs inert Readeck v2 extraction or image decoding.
Cancellation kills and **waits for** the child before the serial queue advances. Successful 202
admission detaches the job from the short-lived open request. A child is a cancellation boundary,
not an OS network or RCE sandbox.

## Run

Go 1.26.8, pinned modules and sums are included. Run all serial checks and both static builds:

```sh
sh scripts/test.sh
```

The default output/cache root is `/root/.hermes/cache/scratch/sticky-reader-service`; set
`SRP_SCRATCH` to another app-owned scratch root if needed. No PlatformIO, firmware, Git, registry,
cluster or credential mutation is performed. Check the parent build-owner completion receipt before
starting resource-intensive commands when integrating into the shared workspace.

```sh
SRP_VERIFIERS=/private/verifiers.json SRP_CACHE=/private/empty-cache \
  /root/.hermes/cache/scratch/sticky-reader-service/artifacts/sticky-reader-amd64
```

Verifier schema: `{"v":1,"devices":[{"id":"device-id","sha256":"<64 lowercase hex>"}]}`.
The hash is SHA256 of the **ASCII 64 lowercase hexadecimal characters** of the bearer token.
No verifier means startup fails. At most sixteen unique IDs and hashes are accepted. The parent
alone generates real credentials and provisions the private native API7 configuration. No token
is returned in content or forwarded upstream. Test tokens in the source are deliberately synthetic.
The cache path must be dedicated to this service. On process startup, only recognized cache files
are purged; unknown files, symlinks and excessive entries cause startup refusal.

Production listens on `:8080` (`SRP_LISTEN` can override). `/healthz` discloses only `{"ok":true}`.
No request/body/header/URL logging, CORS reflection, cookies, redirects or response compression.
Errors use the contract's closed codes. No HTML or origin image URLs are delivered to Lua.

## Verification and fixtures

Normal `go test ./...` includes real HTTP/TLS bytes over controlled streams, real extraction/codecs,
strict malformed protocol tests, principal ownership, quota/queue/cache limits, cancellation,
child crash and forced kill/reap. Only DNS and numeric-to-fixture dialing are fake boundaries.
Set `SRP_TCP_FIXTURES=1` to run the same HTTP tests over actual loopback TCP. This sandbox rejects
both TCP and Unix listeners with EPERM; the default fixture stream mode does not claim to test
kernel TCP or actual public DNS. Never disable production address or authentication checks to test.

`testdata/original.html` and `ready.json` are original synthetic canonical conversion fixtures;
`requests.json` and `responses.json` cover endpoint shapes. `TestCanonicalWireGolden` qualifies
semantic identity including fragments, links, images and forms. Parent freezes/announces their hash
before the companion client adopts a snapshot.

```sh
TMPDIR="$SRP_SCRATCH" python3 scripts/measure.py \
  --binary "$SRP_SCRATCH/artifacts/qualify-amd64" \
  --capture /root/github/.host-data/crosssmudge/browser-cancel/20min.html \
  --output "$SRP_SCRATCH/resource.json"
python3 scripts/benchmark.py --binary "$SRP_SCRATCH/artifacts/sticky-reader-amd64" \
  --capture /root/github/.host-data/crosssmudge/browser-cancel/20min.html \
  --output "$SRP_SCRATCH/benchmark.json"
```

`qualify` is a separate native test driver, not the deployed entrypoint. It uses production service
code and an actual owned conversion executable, synthetic TLS origin/HTTP request bytes, a 3.45 MB
HTML source, 4-million-pixel source image, and optional private captured HTML. It writes counts,
sizes, hashes and timings only. The original article's beginning/middle/end, GET form and visible
index link must survive; image output must be genuine palette 1-bit PNG. The private capture is
never committed or retained as model output. Candidate content coverage is reported without
headlines/body snippets; it is not a claim of universal publisher compatibility.

`measure.py` samples actual aggregate parent/child RSS and checks all observed child PIDs are gone.
Go's 48 MiB parent / 96 MiB child targets are soft. Host RSS measurements do not enforce the pod's
hard cgroup limit or emulate 500m CPU, ARM64 hardware, device SD, Lua or e-ink latency.

## Security and limits

- Sixteen accepted connections and eight concurrent handlers bound pre-auth/header and body state.
  Auth and bounded rates precede body reads, cache or work: 60 operations/minute, separate
  120 polls/minute, ten new opens/minute with burst two and six-second spacing thereafter;
  64 MiB fetched bytes/day/principal, including failed fetches. One active network/conversion task,
  four queued tasks; at most one active distinct open per principal. Identical opens share work
  with independent cancellation. Request IDs, jobs, tombstones and negative entries are bounded.
- Every redirect/image/CSS hop validates HTTPS hostname-only URLs, all DNS answers, public address
  ranges, IDNA, numeric peer pin, original TLS name, system roots and dates. IPv4 is deliberately
  selected; mapped, private and mixed results fail. No environment proxy, credentials or cookies.
  NYT hosts and the backend origin are denied. No JS/headless/login/paywall/bot bypass or feed adapter.
- Shared deadlines and wire/decoded budgets include redirects and failures. Gzip validates CRC,
  length, one-member framing and expansion ratio. HTML is UTF-8 only, max 1 MiB wire/4 MiB decoded;
  token/node/depth budgets precede inert DOM/Readability. External static visibility supports two
  bounded stylesheets and simple tag/class/id rules only, with no imports/resources/layout engine.
- Reader explicitly uses Readeck v2; simplified is an explicitly requested semantic DOM walk.
  There is no automatic fallback. Original visible index navigation and safe GET forms survive.
  Limit-driven omissions mark every chunk `truncated:true,reason:"too_large"`; missing/filtered
  anchors return false. Text positions are one-based UTF-8 byte boundaries; chunks are not screens.
- Images are on demand and doc/principal-owned: JPEG/PNG/WebP, DecodeConfig before decoding,
  dimensions rechecked, 4096 per axis / 4M pixels, white alpha compositing, aspect/no upscale,
  deterministic Floyd–Steinberg and freshly encoded palette 1-bit PNG <=65536 bytes. Unsupported
  formats yield stable errors. Input media is never returned as a proxy response.
- Cache TTL 30 minutes, max 64 documents/global, sixteen/principal, 64 MiB total including
  derivatives, at most 32 image variants/document. Space is reserved under the cache mutex before
  atomic temp/fsync/rename publication. Filenames are keyed digests; no URL paths/queries in names.
  Restart invalidates IDs. Infrastructure operators can observe URLs/content and may have snapshots;
  no application backup is created and no claim is made about infrastructure retention.

## Packaging and parent deployment

`scripts/package_oci.py` creates and validates an actual ARM64 OCI archive without Docker or a
registry. It includes the exact static binary and public CA bundle, numeric UID/GID 65532, entrypoint,
soft runtime defaults and working cache directory. Tests cover content hashes, architecture,
permissions, traversal, configuration and platform. `Containerfile` documents the equivalent
scratch build context; its inputs belong in scratch, not this source directory.

`deploy/app.template.json` is a Kubernetes List containing only the new namespace and app-owned
Deployment/Service/Ingress/NetworkPolicies. It contains no Secret. Render with a reviewed digest:

```sh
python3 scripts/render_deploy.py localhost/sticky-reader@sha256:<manifest-digest> \
  > "$SRP_SCRATCH/app.reviewed.json"
```

Recreate, one replica, 50m/64Mi requests, **500m/192Mi limits**, 64 MiB cache emptyDir and 32 MiB tmp,
nonroot, no service-account token/capabilities/privilege escalation, read-only root, RuntimeDefault
seccomp, app-only egress and existing `letsencrypt-prod` issuer are explicit. IPv4-only public
HTTPS egress excludes special ranges and the known node IP. DNS is restricted to kube-dns;
Traefik and solver label selectors must be read back against the actual cluster before applying.

Traefik v3.6 documents `traefik.ingress.kubernetes.io/router.observability.accesslogs`; the template
sets it false and disables route tracing. Documentation verified 2026-11-16:
https://doc.traefik.io/traefik/v3.6/reference/routing-configuration/kubernetes/ingress/
This is configuration intent; live router logs/headers, certificate issuance, NetworkPolicy behavior,
containerd import, pinned imageID and credentials remain parent deployment qualification tasks.

On a real reTerminal Sticky with the companion API7 lane installed, explicitly import the private
configuration, choose Assisted, open a permitted public page, navigate a later chunk/fragment, Find,
load an image, cancel during fetch/conversion, then check cross-document Back/offline behavior.
Expect explicit errors/Partial when limits apply, retained old view after cancellation, no direct
fallback, and no media origin/token in Lua callbacks. No firmware EPUB cache reset is needed;
this service's ephemeral cache is independent. Parent owns combined native/Lua integration and release.

The native executable embeds eight external compiled modules. `verify_runtime_deps.py` independently
checks every module ZIP's Go h1 digest and extracted bytes against committed go.sum and executable
metadata. Full historical/test-graph `go mod verify` / `go list -m all` additionally requests older
module metadata absent from this sandbox's cache (including x/net v0.33.0); sockets are forbidden,
so that broader audit needs the parent's network-enabled environment. This is not a missing runtime
package or a reason to replace/disable dependencies. Exact failed commands remain in the handoff.

For parent-only live HTTPS qualification, use `python3 scripts/live_flow.py --device-config
/private/device-config.json --url https://example.org/`. It reads an existing private configuration,
refuses redirects, exercises model/chunk/locate/find/image/cancel and prints only sizes/statuses.
It never prints the bearer, URLs, titles, or private body text. The parent must also run the companion
native/Lua tests and physical Sticky flow; this operator script is not device integration proof.

### Repair round 1 qualification

The seven accepted SRP security/model repairs preserve SRP1 and dependency pins.
Oversized single-block metadata returns the stable `too_large` error before publication.
HTTP/1 rejects close without waiting for the withheld request body; accepted requests that
reach EOF can reuse their connection. The HTTP-aware 2048-byte payload limiter uses one
extra byte to detect overflow; tests distinguish chunk framing and header/kernel read-ahead
from intentional body reads. Rejected-body closure expires reads before closing the body.

Deletion and rename/directory-sync failures retain private paths and their reserved bytes.
Unavailable entries and uncertain temporary/renamed files block new cache/job admission;
the existing sweeper retries bounded owned paths. Permissions can prevent physical TTL
cleanup: the service reports failure and stops further growth rather than releasing capacity.
Restart still refuses a nonempty cache; the dedicated-cache reset validates names/types and
syncs the directory, preserving unrelated files. No persistent store or migration was added.

Run `sh scripts/repair_round1.sh /absolute/new-output-directory` for serial normal/race/vet,
package/deploy tests, native/ARM64 builds, OCI validation and native resource measurement.
Use `SRP_TCP_FIXTURES=1` for real kernel TCP qualification on a permitted parent host. Stream
fixtures are explicitly fake connection boundaries and cannot establish kernel TCP acceptance.
The new semantic fixture in `testdata/ready.json` needs parent announcement/reconciliation
with the separately owned API7/Lua client. No combined, ARM64 runtime, live TLS, deployment,
or physical Sticky acceptance follows from host service qualification.
