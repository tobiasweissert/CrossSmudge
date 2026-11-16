# Forehead — local Lua adaptation

Implemented: All 2,474 entries and 17 categories; 30/60/90/120-second rounds; got/missed/unanswered marks; maximum 128-card history; no within-round repeats across deck laps; cumulative, recent and per-category records stored.

Compact-host limitations: Original artwork and graphical record charts are absent. List-based result review. PSRAM hosts use the original-screen path described below. English only. Closed time is paused. Category selection replaces the last completed result display; persisted records remain.

Persistence uses app-relative checked two-slot journals, semantic input validation and readback; failed writes leave the previous slot available. These saves are separate from original CrossPlay and existing CrossSmudge IDs. No firmware or network hooks are installed.

Validation: `test/crossplay_ports/additions_run.py` uses the exact stock int32/float32 Lua VM at 76,800 and 2,097,152 bytes, both orientations, rule fixtures, callback/render instrumentation and journal failures. See repository `docs/crossplay-ports.md`. These are native adapter results, not e-ink screenshots or hardware measurements.

Hardware verification on Sticky v1.6.5.3: Choose each category/length, start with screen outward, mark Got/Missed, pause, expire, inspect results, exit/reopen a full round. Check serial logs for OOM/Application Error, internal free/largest heap and PSRAM. Repeat touch and buttons in both orientations. No EPUB cache reset is needed.

## Original-screen presentation (PSRAM hosts)

The enhanced path ports `ForeheadScreens.cpp` from reference commit `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`: original Jersey 10/14/20/30/44/64 cuts, category icon masks, the portrait front door/category picker/settings, 16-round record bars, counterclockwise landscape ready/word cards, eight-segment countdown, twelve-card result pages and the three illustrated help pages. The main loader keeps the existing compact controller below 512 KiB. Original gameplay/data/save modules are unchanged. Reset requires the armed confirmation to have been drawn before a second activation can clear records. Hiding/returning retains the current save behavior and restores the host orientation on exit.

Run `python3 test/party_ui/run.py` for exact-VM privacy/touch/clock/record/capacity checks and `python3 test/party_ui/capture.py` for actual bitmap-command SVG/PNG captures. Asset provenance is in `art/sources.json`; the repack script and original OFL/Lucide licenses are retained. On Sticky, check category paging, settings confirmation, automatic landscape orientation, Up=Got/Down=Pass, both central touch bands, clock expiry, result paging and score bars. No EPUB cache reset is required.

## Current original Sticky view

The front door, score bars, category pages, settings/reset rows, three illustrated rules pages, landscape ready/live cards, countdown bands and balanced result pages now use original native composition and actual Jersey/category masks. Original ink measurement, font ladder, line leading, glyph centering, discs/rings and button faces are used. Twenty-seven actual-state frames require zero full-frame differences from original builders after entering from either orientation. Actual callbacks exercise category/round length, long cards, Got/Pass/unanswered results, paging, decimal scores 10/16/100, records and reopening.

A UI-only two-slot category journal distinguishes a newly observed zero from an unplayed category; core state is saved first. Reset clears that metadata only after the game reset is durable. If that UI clear fails, starting another round is blocked with a save-failure notice until retry succeeds, so a stale pre-reset counter cannot catch up and resurrect older categories. Corruption/write failure keeps the last valid slot, and reset epochs prevent an old bit from resurrecting after a failed UI clear. Old positive per-category bests can be inferred safely; historical zero-score categories absent from old saves cannot be reconstructed. No game save field is added or rewritten by this metadata.

Non-Sticky and compact branches retain the previous views. Native original word histories are mapped through real IDs/marks for comparison rather than assumed to share a seed/deal order. Both entry orientations restore on exit. Host-rendered equality is not physical display acceptance.

Run `test/social_original_ui/run.py --help` for current native/production pixel checks and `test/social_original_ui/preservation.py --help` for baseline pixels/traces/save bytes. On Sticky, test first/last category pages, round length, zero and multi-digit scores, result paging and long words, all rules, Got/Pass touch and page keys, timer expiry/reopen, and the visible reset confirmation. Repeat SD write failure/retry and both entry orientations without removing existing saves. No cache reset is needed.
