# Reader-core production probes

Run from the integration worktree, with a task-owned evidence output directory:

```sh
python3 test/crosspoint_migration/reader-core/portable.py --output EVIDENCE/portable
python3 test/crosspoint_migration/reader-core/portable.py --sanitize --output EVIDENCE/portable-san
python3 test/crosspoint_migration/reader-core/run.py --output EVIDENCE/codecs
python3 test/crosspoint_migration/reader-core/run.py --sanitize --output EVIDENCE/codecs-san
```

Every compiler subprocess has a 120-second limit and writes command/exit/log receipts. No package installation, SDK initialization, CMake FetchContent, target build, NYT access or hardware access occurs. JPEGDEC/PNGdec are read-only existing dependencies; override `READER_CODEC_ROOT` if the pinned host dependency location differs. The supplied default is the retained owner test dependencies, not a claim that they match the new target dependency pin. Hardware/storage/heap/renderer/FsHelpers and unresolved metadata writing are explicitly doubled; production source files are compiled without editing them for the fixture. TXT's unused cover functions are linker-discarded, not mocked into passing image conversions.

- `portable.py`: real CSS selectors/specificity/dropcaps/RTL/style serialization/cold+warm+partial cache, bounded source input; BlockStyle and render signature fields; both modern and raw-offset legacy TXT facade and durable cache-clear sentinels; real contiguous/segmented uzlib and strict one-shot completion. Hyphenation translation units receive syntax-only checks.
- `run.py`: real miniz/ZIP and PNG/JPEG framebuffer/BMP converter translation units against actual existing JPEGDEC/PNGdec. Exercises streaming/cooperative ZIP, exact-size/truncated/oversize/empty deflate completion, intentional early-stop probes, scratch/OOM retry, gray/alpha/palette/16-bit tRNS flattening, Browser monochrome opt-in, cache-only decode, cancellation and GIF/PNG dimension probing. Normal mode currently reports **31 cases**.
- Full codec sanitizer is **not green**: the existing vendor JPEGDEC fails in `jpeg.inl:1115` on a negative shift during ordinary and progressive JPEG header parsing. `jpeg_vendor_control.cpp` reproduces it using vendor `openRAM` alone, with no Crosscub converter compiled. Exact commands/logs are in the evidence handoff. Only pre-existing vendor alignment exclusions are used; the shift defect is not suppressed. Do not call the full codec suite sanitizer-approved.

These are portable production-core probes, not Expat chapter-parser, whole selected-SDK, target, real-font pixel, hardware/FAT or firmware acceptance. Parent evidence `integration/reader-core/REPORT.md` and `freeze.json` distinguish frozen members from still-conflicted core source.
