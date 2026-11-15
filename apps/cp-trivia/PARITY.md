# Trivia APIv3 preview

New Lua trivia variant inspired by CrossPlay's MIT trivia app; attribution is
retained in LICENSE. This is NOT the original large CrossPlay question pack,
its category/difficulty progression, score history, or reporting system.

One real multiple-choice question is fetched per selection from Open Trivia DB.
No question bank or fallback questions ship in the package. Answers shuffle;
Prev/Next chooses an option, Select opens the full question/answer, Select again
submits, and Back returns to choices. Choice 5 skips. After answering, Select
fetches the next question. Reading screens support pagination and touch footer
controls. Requests have a six-second minimum interval; provider rate refusals
are shown and never automatically retried. Questions may repeat: no session
tracking token is used. Saved question replay starts unanswered, intentionally.

The last valid raw question has two checked local cache slots; corruption or
interrupted writes preserve the previous slot. Device-local timestamp is shown
on cached replay. Frozen firmware still requires its Wi-Fi launch flow; cache
can be used when Internet requests fail, but canceling Wi-Fi exits the app.

API data: Open Trivia DB, https://opentdb.com/api_config.php, under Creative
Commons Attribution-ShareAlike 4.0 International:
https://creativecommons.org/licenses/by-sa/4.0/
The app decodes RFC3986 text and changes answer order; question wording is
unchanged. Cached/downloaded question data remains under that license. The Lua
code is MIT. No provider question data is bundled or published here.

S3-sized preview; no C3 or physical-device qualification. No App Store release.
Public fixture capture and real wire checks remain pending.
