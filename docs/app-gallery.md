# Simulator app-gallery provenance

**Development preview only; not published or source-release approved.**

All 41 owner-store IDs were captured from the frozen real-asset development candidate. An earlier complete published-baseline capture is preserved separately in the handoff. The final gallery does not imply candidate app versions are available in the live store.

## Rendering boundary

This is the genuine native Sticky firmware simulator: production ActivityManager, LuaAppActivity, LuaRunner, GfxRenderer, compiled fonts, real bitmap/glyph/sprite assets and the simulator SDL display backend. It is not a fake-font primitive harness or a PIL rendering implementation. Screenshot BMPs originate in SDL_RenderReadPixels/SDL_SaveBMP. Pillow only converts BMP to PNG and makes the separately labelled contact sheet.

The logical panel is 480×800; the SDK simulator hides a two-pixel bezel on each edge, producing 476×796 unscaled viewport PNGs. Hearts and Forehead use 796×476 landscape. No pixels were invented to pad these captures to nominal panel size.

Simulator-only maintenance fixes adapt the current contact-state API, select Sticky app dispatch for the Sticky simulator profile and link existing backend/resource HAL implementations. A separate capture hook launches the selected app through the real activity manager and disables app networking before runner initialization. The exact [maintenance patch](simulator-gallery/simulator-maintenance.patch), [capture-only patch](simulator-gallery/capture-only.patch) and [patch replay verification](simulator-gallery/patch-verification.json) are retained for reproducibility; they are not silently applied to production firmware. App UI/source/assets were not altered for screenshots. The simulator is not a physical Sticky device; it cannot qualify e-paper waveforms, actual memory fragmentation/PSRAM, touch hardware, SD durability, radios or sleep/wake.

## Safety and data

Every app runs in an isolated Linux network namespace. A probe returned ENETUNREACH (101), app networking is opted out, and the simulator Lua transport fails closed. Weather has no forecast, Browser has no fetched page, Hacker News has no headlines, and Trivia has no provider question. No actual provider calls, NYT content, credentials or private user data were used. Initial saves were empty; all gameplay/utility values shown arise from shipped startup behavior and recorded simulator input.

## Immutable inputs

- Firmware base: `ff825e7732e40057b3ac8bd32e83769ff4480d5b`.
- Published app-catalog base: `286cd07f9466fcf3e5f3dc2f31dd2b4ebd761bc9`.
- Development candidate manifest SHA-256: `4ab3f21dbf54fff3decd8f6096ee67617e4d823d09ffe98191902402323e99cb`.
- Native simulator SHA-256: `9e28f3623e8e926dd8f4b03b124410d22005c7e59247ba3d353956454838e1f9`.
- [Complete image/source/input manifest](images/apps/capture-manifest.json), [build log](images/apps/provenance/build.log), [binary/toolchain identity](images/apps/provenance/native-binary.json), [network isolation probe](images/apps/provenance/network-isolation-test.json).
- [Contact sheet](images/apps/contact-sheet.png) is an overview, not a substitute for the full-resolution PNGs.

## Exhaustive coverage

| App | Candidate version | Actual captured state | PNG | Run log |
| --- | --- | --- | --- | --- |
| Flashcards (`flashcards`) | 0.1.1 | Sample flashcard question | [image](images/apps/flashcards.png) | [log](images/apps/provenance/flashcards.log) |
| Habits (`habits`) | 0.1.1 | Default habit checklist and history | [image](images/apps/habits.png) | [log](images/apps/provenance/habits.log) |
| Pomodoro (`pomodoro`) | 0.1.0 | Idle focus timer | [image](images/apps/pomodoro.png) | [log](images/apps/provenance/pomodoro.log) |
| Weather (`weather`) | 0.1.0 | Offline forecast configuration; no forecast | [image](images/apps/weather.png) | [log](images/apps/provenance/weather.log) |
| Browser (`browser`) | 0.3.2 | Direct reader startup; no network page | [image](images/apps/browser.png) | [log](images/apps/provenance/browser.log) |
| Dice Roller (`dice`) | 1.0.1 | Rolled die | [image](images/apps/dice.png) | [log](images/apps/provenance/dice.log) |
| Life Counter (`lifecounter`) | 1.0.1 | Initial life total | [image](images/apps/lifecounter.png) | [log](images/apps/provenance/lifecounter.log) |
| Holy Rosary (`rosary`) | 1.0.1 | Prayer beads | [image](images/apps/rosary.png) | [log](images/apps/provenance/rosary.log) |
| Divine Worship (`dailyoffice`) | 1.0.1 | Daily prayer-hour selection | [image](images/apps/dailyoffice.png) | [log](images/apps/provenance/dailyoffice.log) |
| Wordle (`wordle`) | 1.2.1 | Fresh Wordle keyboard and grid | [image](images/apps/wordle.png) | [log](images/apps/provenance/wordle.log) |
| 2048 (`2048`) | 1.1.0 | Fresh sliding-tile board | [image](images/apps/2048.png) | [log](images/apps/provenance/2048.log) |
| Sudoku (`sudoku`) | 1.1.2 | Generated Sudoku puzzle | [image](images/apps/sudoku.png) | [log](images/apps/provenance/sudoku.log) |
| Blackjack (`blackjack`) | 1.0.1 | Dealt Blackjack hand | [image](images/apps/blackjack.png) | [log](images/apps/provenance/blackjack.log) |
| Block Drop (`blockdrop`) | 1.0.2 | Falling-block board | [image](images/apps/blockdrop.png) | [log](images/apps/provenance/blockdrop.log) |
| Codex: Ink & Iron (`codex`) | 1.0.1 | Deckbuilder combat | [image](images/apps/codex.png) | [log](images/apps/provenance/codex.log) |
| Water Tracker (`watertracker`) | 1.0.2 | Initial hydration tracker | [image](images/apps/watertracker.png) | [log](images/apps/provenance/watertracker.log) |
| Stopwatch (`stopwatch`) | 1.0.2 | Running stopwatch with split | [image](images/apps/stopwatch.png) | [log](images/apps/provenance/stopwatch.log) |
| Desk Stand (`dashboard`) | 1.0.3 | Desk clock and calendar | [image](images/apps/dashboard.png) | [log](images/apps/provenance/dashboard.log) |
| Hourglass (`hourglass`) | 1.0.2 | Running countdown | [image](images/apps/hourglass.png) | [log](images/apps/provenance/hourglass.log) |
| Solitaire (`solitaire`) | 1.1.1 | Dealt Klondike board | [image](images/apps/solitaire.png) | [log](images/apps/provenance/solitaire.log) |
| Chess (`chess`) | 1.1.2 | Local chess board | [image](images/apps/chess.png) | [log](images/apps/provenance/chess.log) |
| Battleship (`cp-battleship`) | 0.1.5 | Fleet placement | [image](images/apps/cp-battleship.png) | [log](images/apps/provenance/cp-battleship.log) |
| Calculator (`cp-calculator`) | 0.1.4 | Calculator keypad | [image](images/apps/cp-calculator.png) | [log](images/apps/provenance/cp-calculator.log) |
| Checkers (`cp-checkers`) | 0.1.4 | Local checkers board | [image](images/apps/cp-checkers.png) | [log](images/apps/provenance/cp-checkers.log) |
| Connect Four (`cp-connectfour`) | 0.1.4 | Local Connect Four board | [image](images/apps/cp-connectfour.png) | [log](images/apps/provenance/cp-connectfour.log) |
| Forehead (`cp-forehead`) | 0.1.4 | Party prompt/start state | [image](images/apps/cp-forehead.png) | [log](images/apps/provenance/cp-forehead.log) |
| Hearts (`cp-hearts`) | 0.1.4 | Local Hearts table | [image](images/apps/cp-hearts.png) | [log](images/apps/provenance/cp-hearts.log) |
| Hex (`cp-hex`) | 0.1.5 | Local Hex board | [image](images/apps/cp-hex.png) | [log](images/apps/provenance/cp-hex.log) |
| Insider (`cp-insider`) | 0.1.4 | Pass-device role privacy screen | [image](images/apps/cp-insider.png) | [log](images/apps/provenance/cp-insider.log) |
| Knucklebones (`cp-knucklebones`) | 0.1.4 | Dice-column board | [image](images/apps/cp-knucklebones.png) | [log](images/apps/provenance/cp-knucklebones.log) |
| Minesweeper (`cp-minesweeper`) | 0.1.5 | Minesweeper board after cell input | [image](images/apps/cp-minesweeper.png) | [log](images/apps/provenance/cp-minesweeper.log) |
| Notes (`cp-notes`) | 0.1.4 | Default note list | [image](images/apps/cp-notes.png) | [log](images/apps/provenance/cp-notes.log) |
| Wavelength (`cp-wavelength`) | 0.1.4 | Pass-device practice privacy screen | [image](images/apps/cp-wavelength.png) | [log](images/apps/provenance/cp-wavelength.log) |
| Yahtzee (`cp-yahtzee`) | 0.1.4 | Yahtzee scorecard after Roll input | [image](images/apps/cp-yahtzee.png) | [log](images/apps/provenance/cp-yahtzee.log) |
| Jaipur (`cp-jaipur`) | 0.1.4 | Local market and hand | [image](images/apps/cp-jaipur.png) | [log](images/apps/provenance/cp-jaipur.log) |
| Sea Salt (`cp-seasalt`) | 0.1.4 | Local card-draw state | [image](images/apps/cp-seasalt.png) | [log](images/apps/provenance/cp-seasalt.log) |
| Toy Battle (`cp-toybattle`) | 0.1.5 | Local battlefield with starting troop hand | [image](images/apps/cp-toybattle.png) | [log](images/apps/provenance/cp-toybattle.log) |
| Murdle (`cp-murdle`) | 0.1.4 | Case clues | [image](images/apps/cp-murdle.png) | [log](images/apps/provenance/cp-murdle.log) |
| Go (`cp-go`) | 0.1.4 | Local Go board | [image](images/apps/cp-go.png) | [log](images/apps/provenance/cp-go.log) |
| Hacker News (`cp-hackernews`) | 0.1.3 | Offline unavailable state; no headlines | [image](images/apps/cp-hackernews.png) | [log](images/apps/provenance/cp-hackernews.log) |
| Trivia (`cp-trivia`) | 0.1.3 | Empty/offline question state; no provider answer | [image](images/apps/cp-trivia.png) | [log](images/apps/provenance/cp-trivia.log) |
