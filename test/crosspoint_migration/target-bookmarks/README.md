# Target bookmark repair packet

Run from the repository root:

```
python3 test/crosspoint_migration/target-bookmarks/run.py --evidence /ABSOLUTE/EVIDENCE/normal
python3 test/crosspoint_migration/target-bookmarks/run.py --sanitize --evidence /ABSOLUTE/EVIDENCE/sanitized
```

The runner serializes compilation with the shared qualification flock. It uses the existing selected SDK at `634a3f8c067a3714a6a7c622015138af096ab5d2` and installed ArduinoJson without fetching anything. Compiler outputs are confined to `/root/.hermes/cache/scratch/target-bookmarks-build`.

Coverage and limits:
- Real FreeInkApp and project BaseTheme/BookmarkStore headers reproduce the unscoped `UIIcon::Bookmark` value hiding the `struct Bookmark` tag. `BookmarkRecord` aliases that exact tag; it does not introduce a new record layout or change API identity.
- Full production `BookmarkStore.cpp` compiles with the colliding real theme included first, real Serialization and real uzlib checksums. An explicit in-memory disk transport exercises v2-v5 records, exact v5 write bytes, page identity and add/remove behavior. This is not FAT/device qualification.
- Full production `BookmarkFile.cpp` compiles and executes with real ArduinoJson. Only disk/persistable transport is replaced. Tests preserve the upstream flat JSON path, keys, progress hints, optional zero offsets and rename length limit. Binary stores and JSON stores remain separate.
- The complete bookmark-list header/body is compiled after include-only boundary substitution, using actual selected SDK declarations/implementation and actual ButtonNavigator declarations/bodies. No missing SDK member is added to a double. Tests execute selection, stale rows, long press, navigation wrap, deletion/clamp and Back. Graphics/application infrastructure is explicit transport scaffolding, not target-link or visual proof.
- The unchanged legacy bookmark open method executes against actual BookmarkEntry and ProgressChangeResult declarations, checking old page hints and visible-offset navigation even when page hints are stale. It is not a full legacy activity compile.

Baseline RED receipts are retained separately for the real name collision and missing BookmarkUtil include. The helper was inspected in immutable upstream `9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967`; owner `818b0befac4d9249c7abded67fa980187c4b41af` has no BookmarkUtil. Only its path derivation is locally retained, inside owned BookmarkFile.cpp; the unused summary helper is not restored.

No EpubReaderActivity edits or caller changes are necessary: `getBookmarks()` still returns the same underlying vector type, and the existing list construction takes it unchanged. The list's next-release call uses the existing `onRelease(getNextButtons(), ...)` API instead of inventing a missing member. Full Sticky build/link, device persistence and physical interaction remain parent/device acceptance gates.
