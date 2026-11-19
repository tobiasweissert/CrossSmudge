# Lua HTTPS API v3 — PR1 service contract

This is a credential-free, bounded HTTP/1.1 service over verified TLS 1.2 on hardware. It does not reuse the legacy downloader's insecure wolfSSL mode. API v2 names remain available; file access is now confined to each app and its saved-data directory, and binary Lua chunks/host filesystem/process functions are disabled.

## Manifest and network setting

Declare exact lowercase DNS names (no wildcard, port, IP literal, trailing dot, or scheme):

```json
{"min_api":3,"network":{"domains":["example.com","api.example.com"]}}
```

There are at most four domains, each at most 127 ASCII bytes. Unknown top-level manifest fields remain compatible. A malformed `network` object, unknown field inside it, duplicate domain, wrong type, or invalid domain disables network access. A missing declaration leaves an app offline. Network-app saves use `/.smudge/network_data<full-app-directory>/`; they do not import the basename-only legacy save namespace, which could be shared by same-named offline apps in different search roots. Manifest input is limited to 4096 bytes at runner initialization.

Valid network manifests are enabled by default within their declared domain scope. No launch, per-domain, or per-request consent is required, and no domain grants are written. App Settings → App networking enabled — disable is an optional global opt-out; the same row enables networking again. The setting is stored in NVS outside Lua's filesystem. Missing settings default to enabled; storage errors fail closed. Existing per-domain grants are ignored. Relaunch the app after changing this setting. Invalid network declarations fail startup without authorizing partially parsed domains. There is no private-network exception.

The runtime opens base, table, string, math and os for every app, with the restrictions below. A valid nonempty network declaration additionally opens the native Lua UTF-8 library, including when networking is disabled so cached JSON/text can still be decoded. Offline apps retain the previous library set and retained Lua heap baseline. Library initialization is protected against allocation failure; no full standard-library preload is used.

Network apps use the existing Wi-Fi selector when disconnected. Cancelling it exits the app. Wi-Fi acquired by the app is disconnected on exit; pre-existing connections are preserved. Loss of Wi-Fi is an error, never an automatic POST retry.

## Automatic UTC

The first HTTPS request after restart/deep sleep obtains UTC through the existing system NTP sync, or reuses a successful sync from this boot. NTP runs in the HTTP worker, so the Lua/UI loop remains responsive. There is no manual App Settings confirmation. Synchronization is bounded by the existing five-second SNTP wait; failure returns `time_sync_failed` and sends no HTTP bytes. Cancellation is checked before and after synchronization.

The accepted UTC epoch is pinned to monotonic time for this boot. Later system-clock changes cannot move the TLS clock. SNTP is unauthenticated; the synchronized clock, firmware CA bundle and installed Lua app content are trust inputs. The HAL still checks the CA chain, hostname and each supplied peer-chain certificate's validity dates before sending HTTP bytes. The embedded root bundle supplies names/public keys, not root validity dates. Physical SD changes or malicious firmware remain outside this boundary.

## Request and completion

```lua
local id, err = smudge.http_request {
  url = "https://example.com/",
  method = "POST",                    -- default GET; only GET and POST
  body = '{"query":"hello"}',       -- binary-safe string, at most 2048 bytes
  headers = { ["Content-Type"] = "application/json" },
  timeout_ms = 15000                  -- integer 1000..30000; default 15000
}

function on_http(event)
  -- event.id: exact Lua integer request ID
  -- event.status: HTTP status, or 0 before a response
  -- event.body: binary-safe string on transport success (including HTTP 4xx/5xx)
  -- event.error: stable error code on failure; body is absent
  -- Schedule the next request in on_update, not within this callback.
end

local found = smudge.http_cancel(id)  -- false for an unknown/consumed ID
```

Unknown option keys and wrong Lua types are rejected. Safe custom headers are `Accept`, `Content-Type`, and `X-Request-Id` only, case-insensitively; values are printable ASCII and at most 128 bytes. At most four header entries are accepted. Authorization, cookies, host/framing/proxy headers and CR/LF are rejected. No existing user credentials are read or sent. Provider key storage is not part of PR1.

An accepted request returns an ID immediately. Validation/permission/busy/allocation failures return `nil, error` and produce no event. One job globally may be pending, active, or awaiting completion consumption. `on_http` executes inside `LuaRunner::onUpdate` under its Lua mutex. The completed slot is held through callback/OOM cleanup; submit the next request on a subsequent update. Missing `on_http` discards completion. There are no Lua calls from the worker. Shutdown cancels and detaches; the worker holds its native job until drainage and cannot call the destroyed Lua state. No completion is delivered after shutdown. A completion already published may win a cancellation race.

Immediate errors: `permission_denied`, `invalid_options`, `invalid_url`, `domain_denied`, `invalid_method`, `invalid_body`, `invalid_timeout`, `invalid_headers`, `busy`, `out_of_memory`, `worker_failed`.

Completion errors: `time_sync_failed`, `tls_or_network`, `cancelled`, `timeout`, `write`, `read`, `truncated`, `request_limit`, `header_limit`, `protocol`, `body_limit`, `encoding`, `redirect_denied`. TLS/DNS/heap/connection failures deliberately share `tls_or_network`; logs contain codes, not URLs, queries or bodies.

## Transport and limits

- Only HTTPS on port 443. URL ≤512 bytes; path/query ≤384 bytes. A query must follow `/`. No userinfo, fragments, IP literals, IPv6, whitespace, control-byte escapes, backslashes or explicit ports.
- Resolve AF_INET once, reject the answer set if any result is nonpublic (including loopback/private/link-local/CGNAT/documentation/multicast/reserved ranges), then connect to one numeric vetted address. Original hostname remains the TLS verification name and SNI. There is no second hostname lookup. IPv6 is unsupported.
- All 3xx responses are `redirect_denied`; no redirect or credential forwarding. No automatic retries, including POST.
- Response body ≤8192 bytes on devices without PSRAM, or ≤65536 bytes on PSRAM builds. Aggregate response headers plus chunk metadata ≤4096 bytes, line ≤512 bytes. Content-Length and chunked bodies are supported. Compression, interim 1xx responses, chunk extensions/trailers, duplicate Content-Length and conflicting framing are rejected. Close-delimited bodies are rejected: the installed ESP TLS adapter cannot distinguish authenticated close-notify from raw TCP truncation.
- Fixed request/result/scratch/allocator-ledger job, one 6144-byte worker stack, and a worker-scoped 80 KiB/128-allocation **mbedTLS** cap (actual allocator sizes). The cap includes TLS records/certificate work, but excludes esp_tls context, DNS, lwIP, Wi-Fi, Lua and task bookkeeping. A preflight requires 112 KiB free internal heap and a 40 KiB largest block after job allocation. This is conservative admission control, not a measured total native-RAM bound. Device peak heap/stack measurements remain required, particularly C3.
- Cancellation is checked between nonblocking TLS operations. Synchronous SDK DNS may outlast the request timeout; no hard DNS cancellation deadline is claimed. The main Lua loop does not wait for it. Wi-Fi teardown and actual SDK timing require hardware verification.
- `smudge.has_feature("https_async")` is true on hardware. Simulator transport is explicitly unavailable and reports false; it never substitutes fixture responses.

PR1 does **not** include streaming downloads, a JSON decoder, HTML extraction, image conversion, provider authentication, SQLite/zstd, shared file access, or network apps. PR2 must account for these boundaries rather than assume HTTP implements them. Bounded responses can be saved with existing app-local `write_file`; streaming/atomic large downloads need a later API extension.

## Verification and handoff

Run `bash test/lua_http/run.sh` for production policy/parser/manifest/Lua bridge tests under ASan+UBSan. The Lua integration allocator enforces 75 KiB and uses the shipped int32/float32 Lua configuration. LeakSanitizer is disabled because this execution environment uses ptrace. `test/lua_http/openssl_probe.cpp` is a separate host transport harness using the production parser; it is not evidence that the ESP32 TLS HAL works.

Copy `examples/lua-https/` to `/.smudge/applications/https-demo/` on SD. No App Store/catalog change is needed. On Sticky: launch HTTPS Demo, connect Wi-Fi and press Fetch. UTC setup is automatic. Expect HTTP 200 and a nonzero byte count. Back/Home during DNS/handshake/read must remain usable; repeat launch/cancel/sleep/reopen and inspect free internal heap, largest block, worker stack watermark and TLS allocation retention logs. Verify default-enabled launches without consent, disable/relaunch and reenable/relaunch, invalid clock, wrong-host/untrusted/expired/not-yet-valid certificates, private DNS answers and redirects. No EPUB cache reset is needed.

The implementation sandbox denies sockets, so live checks were run separately on the host. Real OpenSSL fixtures passed valid, wrong-host, expired, not-yet-valid and unrelated-CA cases using the production HTTP parser; certificate failures delivered no HTTP request bytes. This is host transport evidence, not validation of the ESP32 TLS HAL. Public numeric-IP connectivity and device lifecycle/heap behavior remain unverified; compilation alone is not device crypto approval.
