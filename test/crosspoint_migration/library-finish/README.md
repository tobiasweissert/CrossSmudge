# Library/index migration qualification

## Routine gate

From the task worktree, serialize these commands (the SDK UI probe shares a scratch directory):

```sh
python3 test/crosspoint_migration/library-finish/prepare.py
python3 test/crosspoint_migration/library-finish/run.py --evidence <normal-evidence>
python3 test/crosspoint_migration/library-finish/run.py --sanitize --evidence <sanitizer-evidence>
```

Every compile/executable child has a 120-second deadline. `prepare.py` downloads only the SHA256-pinned GTest 1.17.0 archive when absent; the selected SDK UI headers/source/Icon.h are extracted from **already-present immutable Git objects** at `634a3f8c067a3714a6a7c622015138af096ab5d2`. It never initializes or downloads an SDK. Set `LIBRARY_SDK_GIT_DIR` if the existing object store is elsewhere. Native inputs need g++, Expat headers/library and Python; CMake is optional and was unavailable on this host, so real qualification uses the direct compiler runner, not an unexecuted CMake claim.

## Executed boundaries

- Six GTest executables compile complete actual Library production translation units and test 155 cases, including external-sort equivalence, the 32767 ceiling, allocation/I/O/rename failures, cancelled publication, metadata reuse/resume, first-seen history, genuine upstream V2 migration and private-directory exclusion. Their fault-injecting HAL is explicitly an in-memory filesystem adapter.
- `ui_data.py` compiles the complete activity/header bodies (only external includes are replaced), linking real selected-SDK FreeInkUI code and actual Library/index code. Renderer/input/manager/recents/settings/BookActions are explicit doubles, not a physical UI. It executes title/series/genre searches, format/completion filters, directions, combined/pure recents, 18-history-row batching, group collapse/expand, settings mutation and failed-save rollback, the empty-history launcher fallback, published-row swipe targeting (including header rejection) and failed-file-delete metadata retention. The source guard checks that consumed settings APIs exist in the actual settings header and derives action/localization enums from current source. This is **not** a fully linked firmware test or real-font/touch qualification.
- `native_disk.py` compiles the real index, metadata reader, ZipFile, miniz InflateStream/implementation, Memory/BuildScratch, Utf8 and Serialization over an isolated Linux POSIX-file HAL. It executes genuine V2 migration, real compressed EPUB ZIP + Expat extraction, interrupted publication retaining the old index, resume from already parsed metadata, 6102-book indexing and byte-identical durable-data canaries. No in-memory ZIP replacement is used in this gate. XTC is deliberately a boundary double in this native-disk probe; the unit/UI probes separately qualify its Library API wiring.
- ASan/UBSan applies to all those executable probes. Physical SD/FAT uncertainty, real device memory/stack/timing, radio ownership, actual font/cover pixels and complete firmware linking remain parent gates.

## Provenance and fixture policy

Imported test/source lineage: CrossInk `9914146eeae7b46b300f475a16c32426fc02ec1f`; direct CrossPoint target `9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967`; owner `818b0befac4d9249c7abded67fa980187c4b41af`. Original repository/license/icon credits remain in place. No NYT content is used.

`upstream-v2.idx` was emitted by the **actual pinned upstream** LibraryBuilder/IndexFile/Format/Text translation units through the same deterministic test storage HAL, using `upstream_fixture.cpp`: build b.txt first, then add a.txt and rebuild. It is not a V7 file with a relabelled header. The checked-in fixture allows ordinary regression runs without extracting baseline sources. The remaining inherited older-layout tests have their original fixture construction; do not represent every inherited header-only downgrade as a genuine older firmware output.

Intentional changes to inherited tests: V7/private paths replace Ink's unversioned V6 paths; source-only metadata reading **must not clear reader caches**, so the old tests requiring a state-preserving cache-clear helper now assert no reader-state mutation at all. Cancellation can interrupt a ZIP/XML read mid-book; only completed metadata is resumable, and the interrupted record is reparsed. Format assertions now expect V7. BookCacheUtils is separately owned and is not replaced or duplicated here.

The root routine includes the UI and disk probes, rather than leaving them as one-off manual executables. Parent must register this subtree's routine in combined CI; no unowned root test/workflow files were edited.
