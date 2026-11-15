# Connections — local API4 development preview

Not published, not in the App Store, and not currently enabled by firmware. The bootstrap requires Sticky/X4 Pro and `https_download_file`; current firmware advertises false because the strict atomic storage contract lacks a proven FAT backend. Do not present this package as a live finished port.

Once that capability is implemented/reviewed, the app explicitly downloads the exact archive used by CrossPlay: `https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json`. No bundled puzzles, credentials, redirects or replacement provider. No live provider call was made in this task. Provider/content usage and redistribution permission are not established by the MIT code license; resolve them before publication. The capture remains external test evidence.

`archive.lua` scans at most 1024 bytes per update and decodes at most one 4096-byte object (depth 8, 256 JSON values). It validates UTF-8/escapes, schema, unique nonempty tiles and dates. It checks the entire array before selecting latest or an exact local device date. The real 2025-04-01 record contains sixteen empty tiles and is skipped as unplayable, never replaced. Difficulty -1 stays unknown. The scanner does not decode the whole archive or retain an index of every puzzle.

The game implements four-word selection, four mistakes, one-away feedback, repeated-guess protection, solved-group removal, shuffle, win/loss reveal and replay-validated restart state. New private save slots are read back before acceptance; failed writes retain the previous valid slot. No legacy save namespace is imported or modified. Local Today depends on the device date, separate from TLS trusted UTC.

Scope limits: latest/date selection only, no original calendar/history/streaks, difficulty coloring or full original layout. The current UI is a paginated text list, with buttons and touch. Category labels are limited to two visible lines; glyph coverage (especially emoji) and orientations need hardware review. Progress is one current puzzle across two backup slots, not an unlimited per-date history. A content fingerprint rejects progress when the provider changes a puzzle with the same date/id; it detects accidental changes, not malicious media modification. No on-glass verification has occurred.

Native fixture/rules/save tests use the captured 1248-record, 1460742-byte archive and the original CrossPlay C++ core as a rules/shuffle oracle. The complete app peaked around 98,039 bytes under a 2 MiB test limit; the full app fails a 75 KiB limit. Therefore X3/X4 are gated before loading modules. The native core/bridge/importer alone passes 75 KiB, which does not establish full app fit. All test heaps return to zero after close.

Run `bash test/lua_http/run.sh`, then `bash test/connections/run.sh` from this repo. Both are host-only; fixture/original CrossPlay paths are configurable or documented test prerequisites. No firmware build is part of these tests.

Code adaptation: CrossPlay `ConnectionsCore.cpp` and its rules semantics, copyright 2026 Mario Ruiz; MIT notice in LICENSE. Bounded JSON decoder adapted from the prior local network-app work. Puzzle/content rights are separate.
