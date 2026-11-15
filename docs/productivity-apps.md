# Productivity apps — App Store preview, 2026-11-15

Five standalone Lua packages live in `apps/flashcards`, `apps/habits`,
`apps/pomodoro`, `apps/weather`, and `apps/browser`. Their layouts follow the
supplied monochrome designs and adapt to the device bounds. They support
physical buttons and touch, including text entry through an on-screen keyboard.
These packages are published to your fork’s owner-test App Store catalog on
`integration/sticky-test-all`. Standard upstream firmware uses the upstream
App Store and will not list this fork’s additions.

Close and reopen **App Store** with Wi-Fi connected to fetch the new catalog.
Flashcards, Habits and Pomodoro appear on all supported devices. Weather and
Browser follow the existing network previews’ PSRAM requirement and appear on
Sticky/X4 Pro; C3 TLS/heap qualification is still pending.

## Install locally

Copy each complete app folder, including its `.lua` files, `manifest.json`, and
`icon.raw`, onto the SD card under `/.crosssmudge/applications/`. For example:

```text
/.crosssmudge/applications/flashcards/main.lua
/.crosssmudge/applications/flashcards/manifest.json
/.crosssmudge/applications/flashcards/icon.raw
```

Eject the card safely and open **Applications** on the reader. Alternatively,
use the device's **Web Transfer** application manager to upload a complete app
folder or package. Existing apps use the same installation process described
in the [app development guide](../apps/README.md#testing--installing-apps).

Flashcards, Habits, and Pomodoro require Lua API v2. Weather and Browser require
API v3 and reuse the existing asynchronous `smudge.http_request` / `on_http`
implementation; they do not add a second network client.

## App behavior and limits

| App | Local behavior | Bounded data |
| --- | --- | --- |
| Flashcards | Study the example World capitals deck, flip a card, advance, add question/answer pairs, rename the deck, and remove cards. | One deck; up to 24 cards; 160 UTF-8 bytes per side; 32-byte deck name; 3,800-byte serialized deck limit. |
| Habits | Add and rename habits, toggle today's checkoffs, view per-habit streaks and a 16-week contribution grid. | Up to eight habits, 40-byte names, and 112 days of daily history. |
| Pomodoro | Start, pause, skip, or reset a focus/break cycle; edit the task and durations. | Focus 1–90 minutes, short break 1–30 minutes, long break 1–60 minutes; a long break follows session four. |
| Weather | Search for a city, fetch current conditions and an hourly/daily forecast, switch overview/detail views, and reopen saved forecasts. | Three city-search candidates; 16 forecast hours and five daily records; CSV forecast response up to 3,800 bytes. |
| Browser | Enter a public HTTPS address or search term, read paginated text, follow extracted HTTPS links, and reopen the last cached page. | Existing 8,192-byte transport response ceiling; up to eight extracted links within a 1 KB link-data budget and six in-session history entries. |

Text limits count bytes, so non-ASCII characters can use more than one byte.
Flashcards and habits save edits/checkoffs rather than writing on every render.
Saved data uses checked journal slots: a failed or interrupted replacement write
leaves the previous valid slot available. A save error stays visible so the user
can retry before closing the app.

Pomodoro keeps the device awake while its timer runs. Each completed phase
stops at the next phase ready to start; breaks do not begin automatically.
Reopening restores the saved phase and remaining time in a paused state. Physical
power-button sleep remains available. Set the device date correctly for habit
history and daily Pomodoro counts.

Use the drawn controls on touch devices. On button devices, use the selection
and previous/next controls; the editor uses the same focused-key navigation.
Each app also provides contextual actions for its options, cards, habits, or
reader pages. Back cancels text entry and backs out of an options screen before
closing the app. In network apps, Back first cancels a pending request.

## Weather and web data

Weather queries `api.open-meteo.com` and `geocoding-api.open-meteo.com`, the two
hosts declared in its manifest. Forecast fields and CSV output follow the
[Open-Meteo forecast API](https://open-meteo.com/en/docs). Weather data is
attributed to Open-Meteo under [CC BY 4.0](https://open-meteo.com/en/terms).
The app displays the provider attribution and the cached/updated state. A failed
fetch keeps the previous forecast available.

Browser sends the requested destination URL to `r.jina.ai`, its sole declared
network host. [Jina Reader](https://jina.ai/reader/) fetches the public destination
and returns text. Markdown notation can remain visible; supported links are
listed in the separate Links menu. This is a public text reader: it does not provide browser
logins, cookies, form submission, or local JavaScript execution. A page larger
than the transport ceiling is rejected with an error; the previous page stays
available. Provider failures and rate limits are also shown rather than replaced
with sample content.

Network apps use the firmware Wi-Fi selector. Before using HTTPS after restart
or deep sleep, sync the clock if needed, verify the displayed UTC against an
independent clock, then select **App Settings → Trust this UTC time for HTTPS**.
The firmware deliberately returns `time_untrusted` until this step is complete.
See [Lua HTTPS API v3](lua-https.md) for the exact domain policy, cancellation,
TLS, and response limits. Its native transport is unavailable in the simulator;
the simulator can exercise UI and saved content, but cannot verify live HTTPS.

## Verification

Run the app behavior checks from the firmware repository:

```bash
bash test/productivity/run.sh
```

The host tests use the shipped int32/float32 Lua build. Their UI/network doubles
and captured provider responses verify app logic without claiming ESP32 TLS or
real SD-card behavior. See the test runner for its accounting of test scaffolding.

For simulator UI checks, mirror the complete app folders into
`fs_/.crosssmudge/applications/`, build the single relevant profile, and always
apply the C3 heap constraint when launching:

```bash
pio run -e simulator
SMUDGE_X3_CONSTRAINTS=1 .pio/build/simulator/program
```

Hardware verification remains required. On **reTerminal Sticky**, check touch
keyboard entry, add/flip/save/reopen cards, check off and rename habits, and run
shortened Pomodoro focus/break/long-break cycles. Check both orientations and
confirm controls remain visible. On **Xteink X4**, repeat with physical buttons,
including navigating every editor key and every action without touch.

For Weather and Browser, connect Wi-Fi, trust verified UTC, fetch a forecast and
a small public page, then reopen their cached data. Test Internet loss,
cancellation during loading, Home/power sleep/relaunch, invalid responses, and
failed SD writes. Check that no completion updates an exited app. Compare free
internal heap, largest allocatable block, and HTTP worker stack watermarks over
repeated sessions; on Sticky also inspect PSRAM. Host success does not establish
device memory or TLS reliability. No EPUB cache reset is needed for these apps.

The five icons are app-local 32×32, 1-bit assets (128 bytes each), stored with the
same 90° counter-clockwise portrait transform as existing apps. Use
[`scripts/convert_icon.py`](../scripts/convert_icon.py) to inspect or convert them.
