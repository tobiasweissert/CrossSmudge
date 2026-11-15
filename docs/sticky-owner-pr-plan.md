# Owner acceptance and PR scope

Do not merge into Tobias's main branch or create/update upstream PRs until Tobias explicitly accepts the candidate.

Trivia is excluded at the owner's request because it cannot be tested. Omit `apps/cp-trivia/`, its catalog addition, and Trivia-specific tests/assets from all proposed PRs. The local owner-test packaging script excludes it. Its already installed copy does not need to be removed to make this scope decision.

Keep the firmware input/network changes separately reviewable from app presentation ports. Prepare app-specific PRs, or small groups sharing a required runtime capability, with original-source comparison, exact Lua checks and owner device results. Gameplay tests alone do not establish visual fidelity.

The owner confirmed Applications scrolling, visible Hourglass Start, the new Sudoku board, and Solitaire behavior/appearance on candidate `92ad891b`. The owner accepted Wordle after the contrast and Back/menu fixes in `cceb94bb`; preserve that design. Automatic UTC setup is included in that candidate. Preserve the supported-device boundaries and document unsupported original features in each app’s parity notes.

Focused candidate `54c1bd1d` is installed and hash-verified. It adds the PSRAM allocator, dark Hacker News text, per-row retries/continuation, and direct article opening. The owner confirms all six story rows load. Hardware logs confirm PSRAM-backed Lua, five uncached metadata HTTP 200 responses and a 19,507-byte article HTTP 200, with approximately 195 KB internal RAM free and a 106 KB largest block before TLS. Final contrast/navigation acceptance is pending. The remaining visual ports and Sudoku records/help/result follow-up are stable and native-tested but are not installed in this focused candidate.
