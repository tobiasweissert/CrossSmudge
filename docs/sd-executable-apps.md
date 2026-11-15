# Sticky SD executable apps (ABI 2; ABI 1 supported)

The existing 25 games, eight apps, and reader stay in the host firmware. On
Sticky, opening Games or Apps also scans `/games` or `/apps` for packages. Copy
an entire package directory, for example `/apps/browser/app.cfg` and
`/apps/browser/app.elf`. Reopen the shelf folder after changing the card.
The ELF is separately compiled Xtensa code; its entry point supplies C callbacks
which own the app's navigation, session, and request decisions. Removing the
package removes that app. There is no built-in Browser or ChatGPT launch alias.
Other board environments keep their original shelf.

Only install native packages you trust. This is not a sandbox: loaded code has
the device's privileges. The manifest hash detects mismatches, not authorship.
ABI validation, file/image/resource limits, and the small import table are
compatibility and robustness checks, not a security boundary. There is no
permission isolation between native modules and firmware.

## Installation and use

Use the ABI2 Sticky host and rebuilt ABI2 Browser/ChatGPT packages. Legacy ABI1 packages remain supported. The host application binary
belongs at its application partition; it is **not** the file to copy into an app
directory. No flashing is performed by the package builder. Preserve your SD
card and key/settings backups before any eventual hardware installation.

Browser supports public HTTPS, inert HTML text, links, 16 RAM history entries,
32 bookmarks, and up to 256 cached pages. Use **Wi-Fi connection**, then **Open
URL**. The menu's cached URLs open offline without making a network request.
Pages are capped at 32 KiB of HTML; scripts, CSS layout, cookies, logins, arbitrary
binary downloads, compressed responses and many large sites are unsupported.
Google size/encoding compatibility is not solved. **Clear browser data** asks
before deleting history, bookmarks and cache; it is not secure erasure.

ChatGPT uses the device's direct OpenAI API connection, fixed to
`api.openai.com`, with `gpt-4.1-mini` for chat and `whisper-1` for speech.
Set the key in **Settings → System → OpenAI API key**, also reachable inside the
app. The editor starts blank and password-masked. NVS storage is not encrypted;
removal is logical deletion, not secure erasure. Keys are never sent to browser
sites, returned through the module ABI, printed in logs, or embedded in packages.
Native code still has full device privileges. Requests may incur provider charges.

Use **Wi-Fi**, edit a draft, and tap **Send**. Hold the voice control while
speaking, then release. Recording is at most 30 seconds; transcription opens a
review editor. Sending the reviewed draft is a separate explicit action. A
failed or cancelled request preserves the draft and conversation. New/Clear
asks before clearing local RAM state; provider records are unaffected. Up to
64 messages / 32 KiB text are kept in RAM, with 8 KiB drafts and 16 KiB replies.
Closing the app or restarting loses the conversation.

Back cancels active work, leaves reading/confirmation views, then returns through
the app's navigation to the shelf. Home also drains work. Native packages are
never automatically resumed after reset or wake; a broken package can be
removed from the card without a boot loop. Load/ABI/hash errors show an error
view with Back available. A native crash still resets the device.

## Build and package format

Run `python3 tools_local/build_sd_apps.py --output /absolute/output/path` with
`PLATFORMIO_CORE_DIR` pointing to the pinned core, or pass its
`packages/toolchain-xtensa-esp-elf/bin` directory via `--toolchain`. The builder
creates `apps/browser` and `apps/chatgpt`, including hashes and a build receipt.
Build host firmware through the repository's serialized `scripts_local/check.sh`
path; do not run competing PlatformIO processes against its shared cache.

`app.cfg` is at most 320 bytes, requires a final newline, and has this exact
version plus six unique fields (order of fields is flexible):

```
CROSSPLAY-APP 1
id=browser
title=Browser
target=esp32s3
abi=2
image=app.elf
sha256=<64 lowercase hex digits>
```

The id must match its directory, use only lowercase ASCII letters, digits,
`-` and `_`, and contain 1–31 characters. Titles are printable ASCII, at most
47 bytes. Unknown/duplicate fields and incompatible versions are rejected.
Up to 256 directory entries are examined and 16 valid packages admitted per
folder, sorted by id. Duplicate titles, including built-in titles, are skipped.
Hide/show uses the shelf's existing title identity. Renaming a title changes that
identity; discovery does not migrate hidden-item preferences.

The host snapshots an ELF of at most 1 MiB into PSRAM, verifies its digest,
checks aggregate allocations/address span (2 MiB), section/symbol/relocation
budgets, and then calls Espressif elf_loader 2.1.0 structural validation and
relocation. The only imported symbols are seven memory/string functions listed
in `lib/ElfLoader/src/CrossPlaySymbols.c`. C++ objects, STL containers, SDK
objects and ownership never cross `AppAbi.h`.

The module receives version/size-checked C services. It owns app decisions;
the host owns display/input, editors, Wi-Fi, fixed-origin provider requests,
public-HTTPS transport, opaque parsed HTML documents, cache, scoped data files,
clock and microphone. Parsed documents are inert data, not executable browser
activities. Strings/events are borrowed only for the documented call duration.
The host copies views before returning and delivers completions on the activity
thread. Worker tasks never call module code.

The app's checked PSRAM allocation budget is 2 MiB in eight slots; parsed history
has a 512 KiB retained budget. Browser's fixed state is about 1.2 MiB because its
256 cached URLs remain independently addressable. The host's large UI model,
request buffer and optional 960,044-byte WAV use checked PSRAM-only allocations;
there is no internal-RAM fallback. The HTML/TLS libraries also have bounded
transient standard-library allocations. Allocation failure in standard-library
internals can still abort with the firmware's no-exceptions build; no claim of
complete OOM containment is made.

On exit/sleep, the host cancels pending work and drains any worker, stops the
microphone, calls module `stop`, then unloads executable memory. DNS and SDK
calls may delay cooperative cancellation; there is no hard exit deadline.
Resources created by a malicious native module cannot be policed by this ABI.

## Verification and hardware acceptance

Focused regression suite:

```
python3 host-tests/sdapps/run.py --output /absolute/test/evidence --modules /absolute/package/output
```

It runs real parser/catalog/app-controller/service code with ASan/UBSan and
host-compiles the official loader to relocate each real target ELF 100 times.
It does **not** execute Xtensa instructions or qualify real TLS, radio, display,
SD power-loss behavior, or microphone wiring. DNS/TLS/I2S boundaries use fakes.
LeakSanitizer is disabled where sandbox ptrace is unavailable.

Before hardware acceptance:

1. Boot the combined host without packages; verify the reader and all original
   25 games/eight apps still appear. Copy both packages and reopen Apps; launch
   each, return with Back/Home, and repeat. Check internal/PSRAM free and largest
   block through Serial between cycles for sustained loss.
2. Try a bad manifest, wrong hash, truncated ELF and incompatible ABI. Expect a
   skipped row or recoverable error view. Restart with a broken package present;
   expect Home, never automatic execution. Restore the matching package.
3. In every supported orientation, check shelf pagination, long Browser titles,
   links/bookmarks/cached rows, full-page reading, Chat bubbles/editor/key screen,
   and all touch targets. Rendered UI variants are not yet qualified by host tests.
4. Visit a small public HTTPS page, use history and bookmarks, disable Wi-Fi and
   open its cached copy. Failed TLS, private destinations, redirects beyond the
   limit and oversized pages must preserve the prior page. Verify explicit clear.
5. Set a test key, send one deliberate message, then hold/release speech. Verify
   microphone power-off, transcript review, no automatic chat send, and explicit
   Send. Test immediate release, 30-second timeout, Back/Home during capture,
   transcription, DNS/TLS and response; listen/check serial for I2S failures.
6. Cancel/exit/reopen repeatedly, sleep/wake and remove packages. Check no stale
   callback, retained mic/radio task or automatic app resume. Confirm key removal
   survives restart; local chat clearing must not claim provider-side deletion.

Source and artifact evidence for this continuation lives outside the repository
under `/root/github/.host-data/reterminal-sticky-chatgpt/crossplay/`. Consult its
HANDOFF for exact build results and unresolved environment/hardware checks.

### Browser and ChatGPT presentation ABI2

The rebuilt Browser and ChatGPT packages declare `abi=2` and require the new
host firmware. Existing ABI1 packages retain their original templates and
service table. Install each complete folder independently under `/apps/`;
replacing just an ELF without its matching manifest fails hash admission.

Browser presents the address, article, inline links and bounded page count.
Tap the address to edit the complete URL, or the menu for Wi-Fi, navigation,
bookmarks, cached pages and RAM visit history. Up/Down or vertical swipes turn
pages. During loading, Back cancels; a failed request preserves the prior page.
The Browser remains an inert HTTPS text reader, without CSS or JavaScript.

ChatGPT presents all retained turns with dark user bubbles and plain assistant
text. The composer remains at the bottom of each page. Hold the microphone (or
Confirm with an empty draft), release to transcribe, review the text, then press
Send explicitly. A nonempty draft disables voice capture. The menu contains
Wi-Fi, key settings, privacy and local conversation controls. Conversations
remain in RAM; there is no saved multi-conversation history. Clear/new chat
retain their confirmation step.

USB mass storage is not supported. This target's CH343 UART bridge and native
microphone pin allocation do not provide the required USB device connection.
Use the existing SD transfer workflow. No USB prototype code is included.

Inline links are measured and drawn as separate fragments, so ligatures do not cross anchors. RTL shaping applies within each fragment; full paragraph-level bidi ordering across links is not implemented.
