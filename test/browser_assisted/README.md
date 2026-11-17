# Assisted Browser qualification

Run from the firmware repository. No PlatformIO command or dependency download
is performed. Use an output directory outside the source checkout:

```bash
ASSISTED_TEST_OUT=/path/to/scratch/normal python3 test/browser_assisted/run.py
SANITIZE=1 ASSISTED_TEST_OUT=/path/to/scratch/sanitized python3 test/browser_assisted/run.py
```

Set `BROWSER_CODEC_ROOT` to the already qualified read-only libdeps directory to
also run actual PNGdec/JPEGDEC/converter checks and production-font UI captures.
`run.py` is also called by `test/browser/run.py`. It records exact commands,
exit status, production-source hashes and the parent build-completion guard.
If a coordinated parent qualification receipt exists, it must have a matching
completed result and released lock; absence from a PID namespace alone is not
proof of completion. `ASSISTED_PARENT_BUILD_GUARD` can explicitly name that receipt.
Without a declared coordination context, ordinary developer/CI runs remain usable.

The real production Lua sources open the same essential/network/work libraries,
with int32/float32 values and a 4 MiB heap. The X3 constraint environment is set;
this harness models the explicitly PSRAM-only Browser budget, not a claim that
Browser fits the 75 KiB C3 envelope. Native API3/C3 and old API6 regressions remain
in the existing routine suites. No `luaL_openlibs` or remote response evaluation
is used. SD and UI fixtures are fault-injected adapters; native HTTP resources
exercise real temporary files. They are not physical SD or panel verification.

Tests cover strict JSON/SRP1 schemas and limits, two maximum chunks plus 24
full-reference history records, actual callbacks/input, delayed poll, links,
GET forms, loaded/saved partial coverage, sequential save, Find, dense-chunk
positions beyond 512 screens, history/rotation, stale handles, cancelled ownership,
layout-before-publication, failed writes/readback, uncertain-root GC, image
references and preserved B5/B1 namespaces. Native tests cover the registered API,
fixed-origin POST/Bearer, ordinary POST/Authorization negatives, every 3xx,
content/framing/gzip/decoded caps, NVS/config faults, entropy eligibility and
worker drain. The true palette PNG fixture is original generated test data.
The codec test verifies PNG → async production decoder → 2-bit PXC → blit with
exactly half of 128×64 pixels black and all file handles closed.

`mutations.py` executes behavioral negative controls against scratch copies:
duplicate JSON acceptance, unsafe uncertain-root GC, broken GET adaptation and
redirect acceptance must each fail the actual tests. It does not mutate source.
Set `ASSISTED_TEST_OUT` to the normal runner's output when replaying.

```bash
ASSISTED_TEST_OUT=/path/to/scratch/normal python3 test/browser_assisted/replay.py --in-process
ASSISTED_TEST_OUT=/path/to/scratch/normal python3 test/browser_assisted/replay.py
ASSISTED_TEST_OUT=/path/to/scratch/normal python3 test/browser_assisted/replay.py --endpoint http://127.0.0.1:PORT --url https://approved-synthetic-origin.example/article
```

The first command replays synthetic HTTP bytes through production HttpCore,
resources, dispatch and real Browser Lua callbacks. The second uses a numeric
loopback socket fixture. The third relays to the parent's actual local backend
using only the explicitly synthetic `a` repeated 64 times test key; use a test
backend verifier, never production credentials. `--response-http PATH` accepts
a frozen actual HTTP response for byte replay. Receipts distinguish these modes.
The client's normal runtime origin and TLS path never change. In this sandbox,
AF_INET socket creation is denied with EPERM: loopback/live relay therefore
requires a parent run outside the sandbox. An in-process fixture is not live E2E
or production TLS proof.

`target_headers.py` is an optional owner-specific syntax diagnostic against the
qualified old Sticky SDK/toolchain headers. It never runs PIO or writes its cache.
Its receipt is explicitly narrower than a full firmware build. The parent must
build/link then-current main and run actual combined E2E and hardware checks.
ASan/UBSan runs set `ASAN_OPTIONS=detect_leaks=0` for the sandbox/ptrace limitation;
no LeakSanitizer pass is claimed. Vendor JPEG and selected PNG inflate objects
retain the pre-existing alignment-sanitizer exception; owned code keeps complete
ASan/UBSan instrumentation.

`feature.py` is part of the routine entrypoint. It compiles and executes the
actual LuaRunner feature/version function bodies and API constant with minimal
host runner fields; the receipt names this scope rather than claiming full
firmware linkage. `recheck_lua.py` can rerun the actual Lua suites without a heavy
rebuild only when the prior qualification's native source hashes still match.
`captures.py` records original synthetic input identities, extra mode/import/form/
image-error frames, and lossless PNG conversions of production-font host captures.
Both helpers require `ASSISTED_TEST_OUT` naming an already qualified output.

Combined qualification uses `combined.py` (after `run.py`, with an owned copy of
Go module dependencies under `COMBINED_SCRATCH/go-mod-cache`). It overlays only
an integration test into the service package; the 42 approved service files are
never edited. Native POST bytes reach the real Go HTTP parser/server, maintained
extractor and owned worker. Actual response bytes return to production HttpCore,
resources, Lua callbacks and the app. The image adapter runs the pinned PNGdec,
production asynchronous PXC conversion and blit, checking 4096 black pixels.
The UI uses actual production font metrics. SD and display remain host adapters.

```bash
ASSISTED_TEST_OUT="$S/normal" python3 test/browser_assisted/combined.py --output "$S/combined-final/normal"
SANITIZE=1 ASSISTED_TEST_OUT="$S/sanitized" python3 test/browser_assisted/combined.py --output "$S/combined-final/sanitized"
# Parent on a normal host; choose a NEW output directory, outside frozen evidence:
ASSISTED_TEST_OUT="$S/normal" python3 test/browser_assisted/combined.py --tcp --output "$S/parent-tcp-run"
```

Default mode is real HTTP/TLS over `net.Pipe` and a framed native test-Io pipe.
`--tcp` uses the same assertions with real kernel loopback HTTP/TLS. Both use
controlled public DNS answers, numeric-to-loopback dialing and a trusted test CA;
neither proves public HTTPS/device TLS. Production fetch policy is unchanged.
The canonical `f6da7173…` fixture runs in the ordinary `run.py` entrypoint,
including image/form anchors and invalid offset controls. The native lane suite
also checks the service's UTF-8 JSON MIME and rejects unrelated parameters.
