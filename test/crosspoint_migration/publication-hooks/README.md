# Publication hooks — host qualification

Run from repository root:

```
python3 test/crosspoint_migration/publication-hooks/run.py --evidence <receipt-dir>
python3 test/crosspoint_migration/publication-hooks/run.py --sanitize --evidence <receipt-dir>
```

The runner extracts the actual WebDAV raw/PUT/DELETE/MOVE/COPY and AirDrop finishFile bodies, plus the complete BookCacheUtils implementation (only includes replaced), and compiles them over explicitly synthetic storage/network/store doubles. It executes 69 cases: acknowledged/failed/uncertain AirDrop book vs package receipt; transaction result routing and collision refusal without cache preclear; acknowledged/failed collection deletion and pending journals; collection MOVE refusal; Boolean file-delete propagation; PUT/COPY publication, preexisting/retained staging and pre/post-mutation negative rename acknowledgements; read/write/sync/close failures; primary and temporary stats-off preservation across three cache families and both clear entrypoints; all four preservation/restore rename faults in before/after-mutation modes; durable cache-clear intent failure and subsequent retry fence; absent clean cache. Source assertions constrain hook placement and all three marker arrays.

The double move/delete APIs test caller dispatch and acknowledgement propagation, not nested transaction internals. The frozen web-files-finish suite owns those actual method/store tests and must remain part of combined qualification. Cache path hashing, actual Txt/Epub/Xtc class bodies, whole target TUs, full firmware, physical FAT alias/power-loss, RF transfer and consent UI are not qualified here. No raw AirDrop certificate header is opened by these tests.

Safety/compatibility restrictions: WebDAV refuses PUT/COPY/MOVE overwrite and collection MOVE rather than deleting destination data or stranding descendant metadata. Existing recovery staging and negative-ack candidates are retained, not cleaned on request abort. BookCacheUtils uses a checked CPCACHECLEAR1 intent at <cachePath>.cache-clear.pending before any preservation rename and retains it after uncertainty; explicit reviewed recovery is required. No automatic retry/boot cleanup is supplied.

FileBrowserActionActivity is a popup only and remains byte-identical. The actual FileBrowserActivity Rename caller is outside this task's exact ownership and still requires conservative-mode/Uncertain adaptation by its owner. See integration/publication-hooks/REQUIRED-OUTSIDE-SCOPE.md.
