# Lua HTTPS file API4 — local follow-up, activation blocked

**This branch implements and tests the portable transfer/store/Lua contract. It does not enable file downloads on hardware or simulator.** `https_download_file` is false; `http_download` returns `storage_unavailable` on these platforms. Do not merge it into a test image as a working download feature. API3 `http_request`, its 8192-byte limit, consent, DNS, TLS and time policy remain unchanged. API4 is additive and requires an integer manifest `min_api >= 4` for its new calls.

The early client contract is `docs/lua-https-file-v4.json` (also frozen in task evidence). The only permitted call is:

```lua
local id, err = smudge.http_download {
  url = 'https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json',
  max_bytes = 2097152, -- required integer, 1..2097152
  timeout_ms = 30000  -- optional integer, 1000..30000
}
-- Success: on_http { id=id, status=200, kind='download', size=bytes }
-- Failure: on_http { id=id, status=status, kind='download', error=code }
local bytes, total_or_error = smudge.download_read(0, 1024)
```

Download is GET only. No path, headers, body, token or credential option is accepted. A single fixed, private downloaded object belongs to the full canonical app directory. Downloads must use API3 domain approval, public address binding, verified TLS chain/hostname/certificate dates and trusted UTC. The existing HAL was deliberately left unchanged; no alternate TLS path was introduced. The tested file sink consumes the same `HttpCore::exchange` parser. Only HTTP 200 commits. Redirects, compression, ambiguous framing, chunk extensions/trailers and header/chunk-metadata overflow still fail. The aggregate 4096-byte framing budget may reject a large response delivered as many tiny chunks; it was not enlarged.

The completion contains no body or native path. `http_cancel(id)`, one global job, main-thread `on_http`, protected Lua OOM delivery and teardown drainage retain API3 behavior. A completed promotion wins cancellation observed afterward. Before promotion, cancellation and the deadline are checked again after file sync/close. Blocking filesystem calls cannot be forcibly interrupted; no hard filesystem latency guarantee is claimed. Missing callback discards the result. The global slot remains held throughout callback execution.

`download_read(offset,count)` uses a zero-based absolute seek every call; there is no retained Lua file handle. Both arguments must be integers, offset 0..2097152 and count 1..1024. Returns binary bytes and total payload length; EOF returns an empty string, past EOF `nil,'invalid_offset'`. Reads while any HTTP slot is owned return `nil,'busy'`. Own cached reads do not require renewed network approval, but require API4 and a native backend. Existing `read_file` and `write_file` cannot address this private namespace.

Additional failures include `api_required`, `storage_unavailable`, `file_write`, `file_commit`, `file_read`, `not_found`, `invalid_offset`, `http_status`; API3 validation/security errors remain applicable. Corrupt or unreadable generation metadata fails closed, without deleting that file. Corrupt media is not silently repaired.

## Store and actual backend blocker

`DownloadStore` requires an **atomic-failure backend contract**: promotion to a vacant generation either succeeds or leaves both names unchanged. Only fixed leaves `0`, `1`, `part` are usable. Each payload has a 20-byte little-endian footer: `CPH4`, nonzero uint32 generation, payload byte length, complement of generation, complement of length. Selection chooses the largest generation; wrap is refused. A new operation removes only the older generation and stale private part, streams into exclusive `part`, writes footer, syncs/closes, then promotes into the vacant generation. Current committed payload is never removed. Maximum service-owned bytes per app: two 2097152-byte payloads plus two footers = 4194344. A failed card may prevent part cleanup; cleanup retries at the next operation. Footer checks are structural, not cryptographic media integrity.

The pinned FAT implementation cannot satisfy the backend precondition:

- `.pio/libdeps/sticky/SdFat/src/FatLib/FatFile.cpp:939–1047` creates a new entry, copies source cluster metadata, removes the old entry, then returns `cacheSync()`. Failure can occur after mutations.
- Thus `rename == false` does not imply unchanged names. Removing a possibly aliased `part` afterward may free destination clusters; a new generation can become visible despite an error result.
- This is a detected-I/O-error issue as well as a power-loss issue. A fake atomic-filesystem test cannot prove SdFat semantics.

The unsafe prototype HAL adapter was removed, not shipped behind an easy-to-enable macro. `Executor::readFile` is absent on hardware/simulator; `Session::configureFileRoot` refuses to configure storage without it. Both advertise the file feature as false. Production `HalLuaHttp.cpp` and the SDK gitlink remain byte-identical to baseline.

**Next integration step:** design and review a recoverable transactional storage primitive against real SdFat failure semantics (including ambiguous rename/sync outcomes), or obtain an atomic backend that meets this contract. Prove selector/rollback behavior with failures at every actual mutation and restart recovery; then implement the HAL adapter, attach `exchangeDownload` to the existing verified TLS Io, and supply its bounded read callback. Merely changing the feature flag or using raw `Storage.rename` is insufficient. Any relaxation to an indeterminate-commit contract requires an explicit contract revision and dependent-client review.

## Memory and verification

No archive-sized native or Lua allocation was added to the firmware core. The existing 8192-byte result buffer is reused for at most 1024-byte writes; the job stores an additional bounded root and transfer metadata, staying within its existing 16 KiB static assertion. A session holds a reusable 1024-byte read buffer plus a 256-byte root, with no per-read native buffer allocation. Native disk adapters may allocate/open file handles per chunk; this is not a no-churn or measured hardware-heap claim. TLS admission/allocation budgets remain those of API3.

Run `TMPDIR=/root/.hermes/cache/scratch bash test/lua_http/run.sh`. It uses shipped int32/float32 Lua, ASan/UBSan, native compilation only. The parser/store suite injects short writes, sync/close/rename failures, disconnect, cancellation, SD removal, TLS-connect rejection, timeout, non-200 status, sync-time cancellation and sync-time deadline expiry. Tests verify 2 MiB cap, malformed framing, metadata uncertainty, previous payload preservation, generation cleanup, and API3 regressions. Async tests cover API4 cancel, close/drain, callback busy, dispatch OOM and read-result OOM.

`download_bridge_test.cpp` uses the production bridge/parser/store, synthetic byte transport and **real Linux disk operations** with `openat`/`O_NOFOLLOW` and `renameat2(RENAME_NOREPLACE)`. It replays the captured 1460742-byte archive, then selects latest using the production Lua importer, under 76800 Lua bytes with zero retained on close. It checks symlink refusal, absolute seeks, EOF, types/caps, API/domain gates and no-body completion. This is not live TLS or FAT validation. The full archive lives in native test memory for replay, never the bounded Lua heap. Captured provider fixture remains outside source/packages.

No PlatformIO, SDK initialization, firmware link, live request or device test was run in this workstream. Historical API3 OpenSSL certificate evidence is inherited, not rerun or represented as new evidence.

## Hardware verification after the blocker is resolved

After independent storage review and coordinator build-lock release, build only the selected Sticky target. On Sticky, verify UTC/consent, fetch the real archive, play Latest and Today, cancel mid-transfer, remove/reinsert SD at write/sync/promotion, disconnect Wi-Fi and leave via Home/sleep. Each failure must preserve the prior committed archive and prior unrelated saves; repeated restart must select the same committed payload. Measure internal heap/largest block, Lua peak and worker stack watermark. Also test wrong-host/expired/untrusted certificates before request bytes, private DNS, redirects and timeout. Test both orientations and touch/buttons. No EPUB cache reset is needed. No C3 app support is claimed.
