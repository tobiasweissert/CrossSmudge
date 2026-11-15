# Forehead — local Lua adaptation

Implemented: All 2,474 entries and 17 categories; 30/60/90/120-second rounds; got/missed/unanswered marks; maximum 128-card history; no within-round repeats across deck laps; cumulative, recent and per-category records stored.

Compact-host limitations: Original artwork and graphical record charts are absent. List-based result review. PSRAM hosts use the original-screen path described below. English only. Closed time is paused. Category selection replaces the last completed result display; persisted records remain.

Persistence uses app-relative checked two-slot journals, semantic input validation and readback; failed writes leave the previous slot available. These saves are separate from original CrossPlay and existing CrossSmudge IDs. No firmware or network hooks are installed.

Validation: `test/crossplay_ports/additions_run.py` uses the exact stock int32/float32 Lua VM at 76,800 and 2,097,152 bytes, both orientations, rule fixtures, callback/render instrumentation and journal failures. See repository `docs/crossplay-ports.md`. These are native adapter results, not e-ink screenshots or hardware measurements.

Hardware verification on Sticky v1.6.5.3: Choose each category/length, start with screen outward, mark Got/Missed, pause, expire, inspect results, exit/reopen a full round. Check serial logs for OOM/Application Error, internal free/largest heap and PSRAM. Repeat touch and buttons in both orientations. No EPUB cache reset is needed.

## Original-screen presentation (PSRAM hosts)

The enhanced path ports `ForeheadScreens.cpp` from reference commit `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`: original Jersey 10/14/20/30/44/64 cuts, category icon masks, the portrait front door/category picker/settings, 16-round record bars, counterclockwise landscape ready/word cards, eight-segment countdown, twelve-card result pages and the three illustrated help pages. The main loader keeps the existing compact controller below 512 KiB. Original gameplay/data/save modules are unchanged. Reset requires the armed confirmation to have been drawn before a second activation can clear records. Hiding/returning retains the current save behavior and restores the host orientation on exit.

Differences still requiring an original-backend comparison: Lua measures rounded glyph advances rather than FreeInkUI's text pipeline, and fits oversized labels instead of silently truncating. A three-digit result uses the next smaller original cut when it cannot fit the original 140px score column. The current core stores category best scores but no separate `everPlayed` bit, so a zero-best category remains labelled NEW. This does not establish pixel-identical firmware rendering or hardware timing.

Run `python3 test/party_ui/run.py` for exact-VM privacy/touch/clock/record/capacity checks and `python3 test/party_ui/capture.py` for actual bitmap-command SVG/PNG captures. Asset provenance is in `art/sources.json`; the repack script and original OFL/Lucide licenses are retained. On Sticky, check category paging, settings confirmation, automatic landscape orientation, Up=Got/Down=Pass, both central touch bands, clock expiry, result paging and score bars. No EPUB cache reset is required.
