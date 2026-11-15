# Insider — local Lua adaptation

Implemented: 4–8 seats; discarded-role variant; private reveal/pass; 300-second questions, pause/resume, voting including Nobody; complete 560-word nonrepeat deck; cumulative and recent records.

Compact-host limitations: Original artwork and ornamental record display are absent. English deck only. Timer resumes paused after closing; no wall-clock time is charged while closed.

Persistence uses app-relative checked two-slot journals, semantic input validation and readback; failed writes leave the previous slot available. These saves are separate from original CrossPlay and existing CrossSmudge IDs. No firmware or network hooks are installed.

Validation: `test/crossplay_ports/additions_run.py` uses the exact stock int32/float32 Lua VM at 76,800 and 2,097,152 bytes, both orientations, rule fixtures, callback/render instrumentation and journal failures. See repository `docs/crossplay-ports.md`. These are native adapter results, not e-ink screenshots or hardware measurements.

Hardware verification on Sticky v1.6.5.3: Deal to all seats, hide each reveal before passing, find the word, vote Nobody or a seat, expire a timer, close on a reveal and reopen privately. Check serial logs for OOM/Application Error, internal free/largest heap and PSRAM. Repeat touch and buttons in both orientations. No EPUB cache reset is needed.

## Original-screen presentation (PSRAM hosts)

The enhanced path ports the original `InsiderScreens.cpp` layout and role artwork from reference commit `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. It includes the live player seats, 16-round record grid, hidden pass-around gate, Citizen/Insider/Master cards, the questions clock/trough, seat-based voting with explicit confirmation, result screen, and five illustrated tutorial pages. Master seats cannot be accused. The last hand-back starts the clock, and hiding a revealed card requests a full refresh to clear its ghost. Original core/data/save modules are unchanged. Hosts below 512 KiB use the unchanged compact controller.

Remaining differences: the existing core starts with four players rather than the original screen's five-player default. The help diagrams describe the original five-player example explicitly. The core does not persist the accused seat, so the result can name an accusation only during the current run. Lua text fitting/rounded advances may differ from FreeInkUI, and native captures do not prove pixel-identical firmware rendering.

Run `python3 test/party_ui/run.py` and `python3 test/party_ui/capture.py`. Provenance is in `art/sources.json`, with original font/icon licenses retained. On Sticky, inspect all three role cards, hide before every pass, start/expire/pause the clock, choose Nobody and a player, verify Master is disabled, and reopen while a private card had been visible. No EPUB cache reset is required.
