# Native AirDrop receive integration

Sticky-only receiver built on the pinned licensed ESP32Drop AWDL foundations.
The implementation is integrated in source; final normal Sticky firmware build
and physical interoperability remain explicit qualification gates. See the task
evidence in `/root/github/.host-data/crosssmudge/airdrop`.

- `core/`: bounded binary-plist offers, strict incremental HTTP framing and
  fresh one-use consent tied to peer, interface and session generation.
- `session/`: actual TLS-session protocol orchestration. Every transfer waits
  for Accept or Decline. A file Upload is authorized on the same live accepted
  TLS connection; reconnects are rejected rather than inheriting consent.
- `stream/`: fixed-workspace DVZip stored/zlib to odc CPIO decoder, one accepted
  regular file of any type, at most **30,000,000 payload bytes**. No whole-file PSRAM.
- `storage/`: bounded streaming to exclusive SD staging, checked length/hash validation and
  publication, immutable recovery journals. Uncertain FAT outcomes retain
  evidence and prevent further native publication pending explicit SD repair.
- `port/`: owning-task TLS lifecycle, interface-isolated listener, bounded
  discovery callback, cooperative AWDL shutdown and Arduino radio restoration.
  A stalled owner remains visible; there is **no automatic restart fallback**.
- `browser/`: immutable pending native URL with attempt-bound, one-use launch
  acknowledgement. The installed LuaBrowser receives it through its normal
  opening path after native ownership stops. Failure retains explicit retry
  or cancel; scheduling a child does not acknowledge delivery.

App policy and File Transfer UI live in
`src/activities/network/AirDropReceiveActivity.*`. Existing outgoing Browser
HTTPS verification remains enabled. Only the inbound Everyone listener uses
its foundation self-signed identity; this makes no verified-Apple-identity claim.

`test/airdrop/` exercises production components, actual pinned SdFat fault
behavior, production coordinator/UI methods and the adapted AWDL lifecycle.
ASan/UBSan are enabled in sanitized runs; LeakSanitizer is unavailable under
sandbox tracing. Fakes at socket, crypto, scheduling and radio boundaries are
identified in the evidence and do not constitute device proof.

Real iOS/macOS discovery (including modern TXT flag acceptance), same-session
Upload behavior, maximum-size SD hashes, memory/stack headroom and repeated
seamless mode switching require physical qualification. Do not claim these from
host tests or the original foundation link-only probe.

## Transfer completion and throughput

Research checked on 2026-11-16 against the pinned upstream receiver and
independent native-AirDrop observations:

- The accepted `Files[].FileSize` is the uncompressed file size. Use decoded
  payload bytes for UI progress; HTTP/TLS/archive bytes include overhead and
  compression, and committed bytes remain zero until verified publication.
- Native senders may wait for a response without sending HTTP's last chunk.
  `Decoder::archiveComplete()` requires the exact consented file, CPIO trailer,
  no pending output/header/record bytes, and a verified zlib checksum when
  compressed. Session then verifies/publishes storage, responds with
  `Connection: close`, and never reuses the unfinished HTTP connection. Silence
  or merely receiving the advertised number of file bytes is insufficient.
- Do not sleep after each buffered archive span. HAL writes already yield;
  an extra 10 ms for each 2 KiB input span delays draining TCP during short
  AWDL availability windows. No measured speedup is claimed before device tests.
- The first verified 2,598,945-byte PDF took approximately 137 seconds from
  staging to verified publication with a 5,760-byte TCP window and six receive
  mailbox entries. A 16,384-byte window with twelve entries took approximately
  200 seconds in a subsequent trial, with 1,207,177 TCP retransmitted bytes
  versus 325,704 in the earlier run; this experiment was reverted. These single
  runs do not isolate RF variation, but they provide no evidence for keeping
  the larger memory budget. The latter run spent about 193 seconds receiving
  and 7.5 seconds verifying/publishing; no local RX-ring or TX-queue drops were
  recorded. Network delivery remains the performance focus.
  Keep RF changes separate from application changes. The S3 receiver uses
  channel 6 and cannot follow Apple's 5 GHz windows.
- Measure the same file with the same sender: receive time, verification time,
  committed path/bytes, minimum internal heap/largest block, and radio drops.
  A successful native sender result plus `Saved ... /Inbox/...` is required;
  a host decoder test does not prove a completed hardware transfer.
- Inbox's book-browser view displays every visible received file type. Files
  without a reader open the existing rename/delete actions instead. Listing
  visibility is separate from format-reading support and from transfer success.

Primary sources:

- [Independent protocol specification, Ask and Upload](https://github.com/t4t5/owly/blob/main/docs/SPEC.md#64-post-ask)
- [Pinned ESP32Drop HTTP completion logic](https://github.com/s-iwaki-d/ESP32Drop/blob/92bdc1221924ca522f3ccbebbd9353612ce5b2d7/ESP32Drop/src/airdrop/core/ad_http.h)
- [Pinned ESP32Drop receiver and streamed decoding](https://github.com/s-iwaki-d/ESP32Drop/blob/92bdc1221924ca522f3ccbebbd9353612ce5b2d7/ESP32Drop/src/airdrop/port/ad_port_esp32.cpp)
- [AWDL availability-window research](https://arxiv.org/abs/1808.03156)
- [ESP-IDF 5.5.2 S3 Wi-Fi throughput and buffer tradeoffs](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/wifi.html#parameters)
- [ESP-IDF 5.5.2 lwIP performance guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/lwip.html#performance-optimization)
