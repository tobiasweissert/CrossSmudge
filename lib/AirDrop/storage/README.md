# AirDrop SD staging and checked publication

The portable `StorageSink` operates through checked `Files` calls. The Sticky
`HalStorageSink` adapter uses only `Storage` and `HalFile`, preserving the HAL's
SPI-before-storage-mutex lock order. There are no raw SdFat accesses in app or
production sink code. The test adapter compiles the unchanged pinned SdFat FAT
implementation against a synthetic sparse block device.

Call `begin(name,size)` only after this transfer's explicit Accept. It reserves
one opaque 128-bit session under `/Inbox/.airdrop/<nonce>/`, writes a checked
Receiving record, and creates `payload.part` with `O_CREAT|O_EXCL|O_RDWR`.
Regular files of any type, including extensionless names, are limited to decimal 30,000,000 bytes. A 1 MiB
free-space reserve is advisory; every actual write/sync/close is still checked.
The original name never selects a directory or executable installation path.

`append` accepts at most 1,024 bytes. It synchronously writes the bounded chunk,
then the HAL adapter yields outside both storage/SPI locks. Reading TLS resumes
only after the sink returns, providing bounded natural TCP backpressure with no
unbounded queue. There is no file-sized or archive-sized allocation. The portable
sink owns 2,192 bytes on the tested host ABI, including a 512-byte journal and
1,024-byte read buffer; the caller allocates this once in the session. Host `-Os`
stack reports have a maximum individual portable function frame of 256 bytes.
Target allocation/stack high-water and physical SD latency remain device gates.

After the stream decoder proves archive/framing completion, `finish` checks
payload length, completes the incoming SHA256, syncs and closes staging, and
reopens it for exact length/hash readback. Payload bytes remain opaque regardless
of extension or sender UTI: PDFs, images,
fonts, archives, scripts and arbitrary binary files are saved unchanged, without
execution, extraction or installation. Content interpretation belongs to the app
that later opens the file. Empty files are supported.

New receives save exactly `/Inbox/<validated original basename>`, preserving UTF-8
bytes, case and extension without a postfix. Nonces remain private staging identities.
Known destination collisions refuse before staging; the prepublication check also
refuses late collisions. Existing files are never overwritten or removed. A healthy checked parent-directory scan must
report the target missing, then publication intent is durably recorded before
one no-overwrite rename. Both pinned FAT and exFAT rename use `O_EXCL`; the
actual FAT fault suite qualifies the FAT implementation only. A failed rename
can already have created shared-cluster aliases. The core performs no further
I/O after that false result, and never removes or retries either possible name.
After true, final length/hash and healthy staging-name absence are checked,
then a PublishedVerified receipt is synced/closed/read back. Only then does
`finish` succeed and `committedBytes()` become nonzero. Parent protocol code
must wait for this success before its final sender acknowledgment.

Four immutable 512-byte records under each session are named `phase.1` through
`phase.4`: Receiving, CompleteStaged, PublishAttempted, PublishedVerified. Every
creation is exclusive, every write/sync/close is checked, and every record is
read back byte-for-byte. New layout version 2 and legacy layout version 1 share magic `ADSTG001` at 0, version at
8, phase at 9, little-endian payload size at 12, SHA256 at 16 (zero in phase 1),
121-byte safe name at 64, 192-byte final path at 185, 32-byte nonce token at 400,
and CRC32 of bytes 0..507 at 508. Remaining bytes are zero on creation. The
final path is reconstructed and compared during recovery, never blindly trusted.
Version 2 reconstructs the exact original name; version 1 retains the historical
`<stem> (AirDrop <nonce>)<extension>` path. Mixed/unknown versions reject. Legacy
records remain read-only and are never implicitly republished.

Missing, short, torn, CRC-invalid, gapped, mismatched or unreadable records fail
closed. A persisted phase 3 without a checked phase 4 is publication uncertainty;
recovery never retries rename, deletes aliases, or equates matching hashes with
independent cluster ownership. Such a session requires explicit SD repair.
`inspect(token)` is read-only and rehashes content. `inspectJournal(token)` checks
all journal identities/order/CRC/digest consistency but treats phase 4 only as
a historical durable terminal receipt, so normal entry need not reread every
previous 30 MB file. It does not claim the payload still exists or is unchanged.
The HAL's sequential recovery iterator is bounded to 256 session directories;
call index 256 for the final EOF/quota probe. Unreadable/excessive/unrecognized
inventory produces an explicit failure rather than false absence.

Cancellation is sticky for a HAL sink's owner lifetime and cannot be erased by
a racing begin/recovery call. Only `requestCancel` crosses tasks; all other sink
methods, including file close, run on the SD/listener owner. Join the owner
before destruction. Cancel/failure retain prepublication `.part` files and
records rather than risking destructive cleanup after an SD fault. These are
hidden staging candidates, never final received files. No global SD latch
or mutation of other apps, saves, caches, readers or transfer modes occurs.

Tests: `python3 test/airdrop/storage_run.py [--sanitize]` runs the production
stream decoder into this sink for exact 30,000,000-byte stored/zlib cases,
content validation, journal recovery and 56 adapter-operation failures.
`python3 test/airdrop/storage_run.py --fat [--sanitize] [--sdfat <qualified src>]`
runs actual pinned FatLib/common sources with sector-write prefixes, block-read
and device-sync failures. It reproduces remounted shared-cluster aliases and
asserts preservation plus no failed-rename follow-up I/O or retry. Host OpenSSL
provides the test digest adapter; Sticky uses its pinned mbedTLS SHA256 API.
ASan/UBSan run with LeakSanitizer disabled under sandbox tracing. These tests
are not physical SD, exFAT, controller power-loss, iOS26, or Sticky proof.

A healthy historical phase-1 journal is `RetainedPartial`, not a live
`Receiving` writer. Inspection leaves every candidate byte unchanged; a fresh
consent may reserve a different nonce. Active stages and real publication
uncertainty still refuse reuse, and uncertainty cannot be cleared by inspecting
another historical candidate. Journal phases stay unchanged; new sessions write version 2, including after
read-only inspection of a retained version-1 partial.
