# Hacker News original presentation and retained service

Sticky uses the original CrossPlay Jersey/Noto font cuts, list geometry and
reader layout from revision `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`.
`test/hackernews_original_ui/run.py` exercises production callbacks and pixels:
Busy and Reader match complete native frames in both orientations; List matches
outside the footer label band described below. Original font licenses and
`art/sources.json` identify the actual repacked bytes.

The accepted service remains Firebase Top/New/Ask/Show with articles through
Jina Reader, comments, checked caches, cancellation and per-row retry. Article
responses are bounded at64KiB and comment responses at128KiB on the qualified
S3 path. Long article endings and repeated pending/active More navigation are
covered by the existing repair regressions and new runtime tests. Provider
content keeps its original rights; no live feed data ships here.

CrossPlay's Algolia Front Page/Saved service is absent from this port. The native
footer labels therefore read TOP/FEEDS, with the current selected feed, and the
count retains the existing direct-comments action. There is no invented Saved,
SAVE or UNSAVE service. Request/cache status uses the empty body gutter. ASCII
and Latin1 masks are original; unsupported Unicode is displayed with explicit
codepoint escapes while provider bytes remain intact. These are scoped
presentation comparisons, not complete original service equivalence.

Missing device identity and non-Sticky devices retain the baseline presentation
body. Controller/model/cache/JSON and network manifest remain unchanged. The
new masks load once (55182fontbytes); the largest new production fixture peaks
at614601bytes under the2MiB Lua limit. No C3 full-data qualification is claimed.
Host raster tests do not establish physical e-ink or live TLS acceptance.

For hardware verification, merge local candidate application files in place
without deleting caches or saves. On Sticky, test cached/cold startup, all four
feeds and landscape pages, title/comment actions, buttons and touch, long
article endings, repeated More, cancellation, failed writes, exit and reopen.
No cache reset is required. Firmware Wi-Fi launch policy still applies.
