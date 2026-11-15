# Owner acceptance and PR scope

Do not merge into Tobias's main branch or create/update upstream PRs until Tobias explicitly accepts the candidate.

Trivia is excluded at the owner's request because it cannot be tested. Omit `apps/cp-trivia/`, its catalog addition, and Trivia-specific tests/assets from all proposed PRs. The local owner-test packaging script excludes it. Its already installed copy does not need to be removed to make this scope decision.

Keep the firmware input/network changes separately reviewable from app presentation ports. Prepare app-specific PRs, or small groups sharing a required runtime capability, with original-source comparison, exact Lua checks and owner device results. Gameplay tests alone do not establish visual fidelity.

The owner confirmed Applications scrolling, visible Hourglass Start, the new Sudoku board, and Solitaire behavior/appearance on candidate `92ad891b`. Wordle needs darker hints and its new design on the Back/menu path. Hacker News needs automatic UTC setup rather than a manual App Settings confirmation. Parallel agents are implementing the remaining original CrossPlay screen layouts; document unsupported original features instead of simulating them.
