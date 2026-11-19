# AirDrop stream core (not integrated)

`Decoder` accepts an already consented single-file `Manifest` and DVZip body
bytes. It does not open SD files, acknowledge AirDrop, publish books, or drive
Wi-Fi/UI. The caller must establish bounded HTTP framing, authorization,
absolute transfer/stall deadlines, and safe publication before sender success.
All errors expose a static reason through `error()`; the integration boundary
must log that reason before reporting failure.

The mandatory `archivePath` binds the member to exactly the consented basename
or `./basename`; it never selects an SD destination. One regular file of any type, optional root `.` directory, and one matching `._basename`
AppleDouble sidecar are supported. Directory wrappers are rejected. The sidecar
must use the same `./` prefix as the accepted path and is discarded, never
extracted. Permission bits are not applied to saved files. Links, special files, unexpected or
duplicate files, unsafe FAT basenames, invalid UTF-8 and bidi controls fail.
Bytes remain opaque: this core does not establish EPUB validity or text encoding.

DVZip stored and independent zlib records feed a strict incremental odc CPIO
parser. The zlib inflater is the existing repository `crosspoint_tinfl_*`
implementation via `MinizConfig.h`, without ROM symbol collision, a new miniz
copy, the `InflateStream` framebuffer loan, or its hidden unused-input state.
Zlib stream end, exact compressed-record consumption and Adler32 are checked.
A CPIO trailer is mandatory; at most 511 zero padding bytes follow it. Explicit
DVZip zero end records are supported. A complete record boundary is accepted
after the full CPIO manifest/trailer validates, even without HTTP's final zero
chunk: native senders can wait for our response before sending it. The owner
must then close the connection after publication rather than parse another
request. Silence never signals completion. Gzip, raw CPIO and newc are unsupported and have no fallback.

Bounds are decimal 30,000,000 payload bytes, 32,000,000 archive bytes,
40,000,000 DVZip body bytes, 1,048,576 encoded/expanded bytes per record,
16,384 records, 16 CPIO entries, 1,000,000 metadata bytes and a 65,536-byte
AppleDouble sidecar. No compression-ratio limit rejects ordinary compressible
text. The caller separately bounds HTTP wire/framing bytes and elapsed work.

`feed` reports exact consumed input. The all-or-none sink receives at most
1,024 bytes per call. `WouldBlock` retains unaccepted bytes in the caller or
fixed decoder window; retry with the unconsumed input. No allocation occurs in
`reset`, `feed`, `finish` or the low-level inflater. The caller-owned object is
41,864 bytes on the tested Linux ABI, including the 32 KiB history window; do
not put it on a task stack. A Sticky owner must admit and allocate it once,
join all producers before freeing, and measure real target memory high-water.

Run `python3 test/airdrop/stream_run.py` and the same command with `--sanitize`.
Tests compile this production decoder with the unchanged repository miniz;
host zlib generates synthetic fixtures only. Exact 30,000,000-byte stored and
zlib cases stream generated chunks through backpressure and compare count/hash.
Tests cover malformed/truncated/checksum/path/link/duplicate/quota cases and
byte-split metadata. They do not prove actual Apple peer record shapes, an SD
write, FAT publication, hardware memory use, or iOS/Sticky interoperability.
ASan/UBSan run with LeakSanitizer disabled because the sandbox's tracing blocks
LSan; the original failed LSan attempt is retained in task evidence.
