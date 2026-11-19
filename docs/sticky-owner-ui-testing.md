# Sticky owner UI candidate

This is an unpublished test snapshot. Keep `main` and upstream pull requests unchanged until Tobias accepts it. App manifest versions are unchanged; identify the snapshot by the paired firmware version and `app-file-inventory.json`. Reinstalling apps from the published App Store can replace these local test files.

## Device checks

1. **Applications:** swipe up through multiple pages, swipe down to the first page, and try both ends of the list. A swipe must scroll without launching an app. Tap an app after scrolling. Check buttons too.
2. **Icons:** inspect Forehead, Insider, Wavelength, Sea Salt and Hacker News in the Applications launcher. They should be upright. App Store thumbnails are a separate path.
3. **Hourglass:** tap visible Start, Pause and Reset controls. Open Set, adjust both increments, and tap Done. Try portrait and landscape, then check the side buttons.
4. **Sudoku:** open New Puzzle, generate each difficulty, and cancel one generation with Back. Pick digits on the three-by-three pad, tap cells to enter/clear, tap a clue to pick up its digit, and hold an empty cell to add/remove pencil marks. Try Undo and Hint, including a wrong entry. Exit and reopen to check the board and elapsed time. The previous Crosscub save remains separate. Check the difficulty drawer and How to Play.
5. **Solitaire:** use the stock, move a tableau run, and move a card to a foundation. Reject an illegal move. Open New game for draw-one/draw-three, Undo and Auto-finish. Cancel a new deal and verify the old board stays intact. Exit/reopen. Inspect long columns and the footer.
6. **Five Letters:** enter a five-letter guess, press Enter, delete a letter, and hold backspace to clear the current entry. Tap the title for the menu. Inspect repeated-letter feedback and keyboard states. Daily play still requires the original answer/word archive; without it, use the practice game. Exit/reopen mid-guess.
7. **Hacker News:** connect Wi-Fi; UTC should synchronize automatically without an App Settings prompt. Load all six rows in Top/New/Ask/Show. Tap a title to open its article directly and page through it. Tap a comment-count badge to read nested comments and load more replies. Retry a failed row and confirm other stories still load. Try a self-post with no external URL. Cancel an in-flight request with Back; a late response must not replace the new screen. Reopen offline to check saved stories/articles. Articles above the device limit must show an error.
8. **Trivia is excluded from the PR scope:** no acceptance test is required for it.
9. **Regression:** play 2048 and Minesweeper, including a flag hold; check Desk Stand and Water Tracker touch controls. Exit apps and reopen after sleep.

Record the firmware version, app, orientation, action, expected result and actual result for each failure. A photo is useful for visual differences.

## Verification boundary

Native tests exercise the shipped Lua VM, rules, persistence and input callbacks. SVG/PNG captures replay actual bitmap drawing calls, but do not establish e-ink refresh quality or physical touch accuracy.

The new Solitaire, Five Letters and Hacker News layouts follow the supplied references. Sudoku now ports the original board geometry and Jersey masks. Complete original-screen parity is still pending for the remaining migrations; their `PARITY.md` files list gameplay and presentation gaps. Sudoku's statistics/result ornaments and five illustrated help pages are native-tested in the combined source branch; the installed focused build retains the previously accepted Sudoku version. Do not describe the entire migration as visually identical yet.
