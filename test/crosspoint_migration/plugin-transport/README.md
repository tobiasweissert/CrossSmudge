# Plugin transport preparation and owner diagnostic candidate

This is **not** a qualified adapter or restored secure Hub. Generic declarative
plugin/event/relay requests stay closed unless separately qualified. Neither
`sticky` nor `sticky-owner-test` enables `CROSSCUB_VERIFIED_PLUGIN_TRANSPORT`.
Permission still defaults to allowed; qualification is a separate boundary.

## Registered host entrypoint

```
flock /root/.hermes/cache/scratch/crosscub-review-probes.lock \
  python3 test/crosspoint_migration/plugin-transport/run.py --evidence <private-directory>
```

The entrypoint runs the original qualified/unqualified normal and ASan+UBSan
modes (all with `_GLIBCXX_ASSERTIONS`), two **unqualified** diagnostic modes, then
four caller modes. `caller_run.py` links the actual pinned SDK `JsonListParser`
and `StreamingJsonParser`; it requires the read-only SDK and ArduinoJson header
paths declared at its top. It does not download or copy the SDK.

Boundaries:
- Adapter tests execute extracted production Connection/exchange/wrapper bodies
  over explicit DNS/ESP-TLS/HAL/storage doubles. They do **not** qualify crypto.
- Caller tests extract unchanged manifest/index loading, notice callback,
  start/fetch/retry methods, compatibility helpers and the three-pass bounded
  update-first ranking body. UI, notice-marker persistence, authentication,
  template-free fixture substitution, empty installed-version inventory and file
  streaming are explicit seams. Real JSON and SDK parsers process captured bytes.
- The captured Hub 0.1.17 `lists_url` points at `catalog-lists.json`; the notice's
  target is **community-catalog.json**, not the default catalog.json. Its 38 raw
  rows produce 36 eligible rows and pages 16/16/4 with the actual unchanged
  firmware capabilities. No unsupported capability is invented.
- An injected catalog TLS failure **after successful index loading** reproduces
  notice acceptance -> ERROR -> retry -> parsed pages. It does not identify the
  installed device's failure cause. On current default-closed source, remote
  index loading fails before any notice and retry remains closed. The installed
  Mac build, exact Hub bytes and failure stage remain unknown.

Fixture provenance (public, previously captured bytes, not generated schemas):
- `fixtures/v0.1.17-device.json`: preserved version-specific Hub device.json from
  the owner task's `plugin-hub/v0.1.17-device.json`.
- `fixtures/catalog-lists.json`: `/root/crosscub-plugin-hub/catalog-lists.json`.
- `fixtures/community-catalog.json`: exact captured
  `/root/crosscub-plugin-hub/community-catalog-audit.json` (26,684 bytes).
  Receipts hash all fixtures and extracted production inputs.

## Owner-only on-device diagnostic candidate

Only the already-approved packed `sticky-owner-test` profile adds
`CROSSCUB_PLUGIN_TRANSPORT_DIAGNOSTICS=1`. Manual non-distributing CI can select
that profile; default/PR builds remain `sticky`. Existing encrypted artifact,
analysis, image, startup and publication gates are unchanged.

After parent source approval, a real target build and identification of its exact
private BIN/ELF/source hashes, connect Wi-Fi explicitly, return Home and send:

```
CMD:PLUGIN_DIAGNOSTICS\n
```

This is an ASCII serial line with a newline, not a shell command. Nothing probes
on boot or from saved settings. The production USB line handler accepts the exact
command only at Home. There is no URL argument, arbitrary endpoint, auth/body,
install, config mutation or native diagnostic browsing lane.

The private admission inside PluginHttp accepts only compile-fixed anonymous GETs:
0 Hub index, 1 default catalog, 2 community catalog, 3 expired.badssl.com,
4 wrong.host.badssl.com, 5 self-signed.badssl.com, 6 untrusted-root.badssl.com.
No redirects are followed in this lane. Every target uses the **production**
Connection/exchange, bundle, hostname/SNI, public IPv4 validation, numeric TCP
pin, TLS 1.2 and explicit certificate-chain date checks. No second TLS client,
test trust anchor, setInsecure or SSRF exemption exists.

Each observation is one bounded JSON record (no response body retained), with
fixed build schema, sanitized source SHA/dirty state, probe index, clock-ready,
numeric stage, verify flags, HTTP-write call/byte counters, HTTP status,
completion and elapsed milliseconds. HTTP counters are incremented at each
successful actual `esp_tls_conn_write`, including partial writes before failure.
Stages: 0 input, 1 clock, 2 DNS, 3 address, 4 TLS, 5 verification, 6 date/chain,
7 HTTP. Records always say `qualification: UNQUALIFIED`; they never self-certify.

Limits: 32 KiB response per probe, normal bounded headers/lines/chain, 30-second
**cooperative** elapsed budget per probe, no retries or redirects. Existing HAL
NTP mutex/DNS and resolver calls are synchronous and can exceed that budget.
A strict wall-time bound is **not established** by this source or host doubles;
record this as a device/architecture acceptance limitation, not a measured pass.
NTP is unauthenticated time, not an authenticated clock service.

## Physical acceptance (still BLOCKED)

For each bad-certificate fixture, require `http_writes == 0` and
`http_bytes == 0`. A positive write is FAIL even if a later call fails. A zero
counter alone is not a certificate PASS: DNS failure, timeout, missing Wi-Fi,
clock failure or unknown TLS handshake failure is BLOCKED. Use real verify flags
and native serial evidence to distinguish actual cert rejection; record flags
that the platform does not expose as unknown, not guessed reasons.

Require successful real anonymous Hub index/catalog responses and all applicable
negative tests on the **same identified target artifact**. Future-dated leaf and
expired-intermediate public fixtures are not supplied here and remain MISSING.
No complete secure-Hub/stable acceptance is implied. Physical device runs,
real ESP TLS/date/chain behavior, strict timing and unknown installed Mac state
remain unverified. Parent alone dispatches CI and decides artifact admission;
no stable flag, version, main/tag, publication or owner settings change is made.
