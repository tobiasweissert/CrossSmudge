# Web/settings + complete file transaction composition

Run `python3 test/crosspoint_migration/web-files-finish/all.py --evidence <DIR>` and repeat with `--sanitize`.

This executes actual extracted GET/POST methods with actual ArduinoJson and SettingInfo, actual file-mutation orchestrator/cache-delete method bodies with storage/store doubles, actual bookmark codec/preparation/commit bodies with the inherited FsFile double (explicit HalFile spelling alias), and the unchanged sibling full ClippingStore TU/catalog/security/signature suites. It does not install/initialize an SDK or build firmware. Every compiler/child is bounded and receipts persist source hashes, commands and exit statuses.

The orchestration double proves phase ordering/failure propagation, not nested store durability; separate real bookmark/clipping tests exercise nested codecs and recovery. The FAT double models negative before/after acknowledgements and alias-like candidate retention, not physical power loss. Clipping v5 tests carry the actual tombstone and retained-root semantics.

Source integration contract and frozen payload are under the task evidence `integration/web-files-finish/`. Pending recovery is intentionally fail-closed; tests do not delete the last recovery candidates to make retries appear green. Reader sessions must be flushed/closed before the file-only web entrypoint runs. Parent owns UI caller adaptation, generic BookCacheUtils temp preservation and full target acceptance outside the seven source paths.
