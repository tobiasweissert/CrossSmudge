# Crosscub ✒️

**Crosscub** is an open-source, extensible e-reader and micro-application firmware for the Seeed reTerminal Sticky (ESP32-S3R8, 16 MiB flash, 8 MiB PSRAM).

Built upon the typography and reading enhancements of **[CrossSmudge](https://github.com/Mumfee/CrossSmudge)** and **[CrossInk](https://github.com/uxjulia/crossink)** and the rock-solid foundations of **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)**, **Crosscub** preserves the inherited lightweight **standalone Lua 5.4 application engine**, an **on-device App Store** for wireless app installations, a **browser-based web app manager**, and an expanding suite of games and utilities—all engineered to run within the strict hardware and memory limits of low-power e-ink hardware.

---

### Supported Devices

- **Seeed Studio reTerminal Sticky** (ESP32-S3, touch, PSRAM)

---

## What's New in Crosscub 🎮✨

### 🚀 Standalone Lua 5.4 Application Engine
Develop, share, and run custom applications directly from your SD card—**no firmware compilation or flashing required!**
* **Dynamic Discovery:** Drop an application folder into `/.crosssmudge/applications/` on your SD card, and it instantly appears in the on-device **Applications** launcher.
* **Full E-Paper Graphics API:** Complete `smudge.*` standard library providing high-contrast vector drawing, text wrapping, bitmap sprite rendering, modal dialogues, font selection, and hardware button/touch event handlers.
* **Hardware-Enforced Memory Safety:** Sandboxed Lua runtime with the existing per-app heap budgets and PSRAM policies preserved.

### 🛒 On-Device App Store (Over Wi-Fi)
Discover and install community-created applications directly on your device:
* Connect to Wi-Fi and open the **App Store** from the Applications menu.
* Browse the official community catalog hosted on GitHub with rich descriptions, version tags, author credits, and 64×64 preview icons.
* One-click **Install**, **Update**, and **Uninstall** directly over the air without connecting to a computer.

### 🌐 Web Portal Application Manager
Manage your device's apps from any desktop or mobile browser:
* Connect your device to your local Wi-Fi network and visit `http://<device-ip>/applications`.
* **Drag-and-Drop Installation:** Drag app directories or `.zip` bundles straight into the browser window to install them immediately.
* **Storage Manager:** View installed apps, versions, author information, storage footprint, and delete apps with one click.

### 🃏 Built-In & Community Applications
Crosscub includes a collection of e-paper applications and games:

| Application | Description |
| :--- | :--- |
| **Desk Stand** | Multi-layout ambient e-ink desk companion with 7 portrait and landscape styles, digital flip clock, daylight/year progress bars, monthly calendar, and auto-sleep suspension. |
| **Stopwatch** | Pixel cartoon stopwatch with mechanical crown plunger, workout facial expressions, digital LCD window, and comprehensive lap split recording. |
| **Hourglass** | Visual countdown sand timer with animated parabolic sand funnel crater, falling sand stream, conical sand mound, and cute expressive face. |
| **Solitaire** | Classic 7-column Klondike card game with stock, waste, foundations, and smooth multi-card dragging. |
| **Water Tracker** | Daily hydration tracker with an animated pixel cup character, configurable volume units, and 14-day history archive. |
| **Codex: Ink & Iron** | Full roguelike deckbuilding card RPG featuring 7 animated encounters, card rewards, relics, events, shop, and scriptorium. |
| **Blackjack** | Authentic casino blackjack with split pairs, double down, insurance, dealer AI, custom face-card sprites, and persistent bankroll. |
| **Divine Worship** | Complete daily prayer companion and liturgical psalter for the Anglican/Personal Ordinariate tradition with automated liturgical calendar computation. |
| **Block Drop** | E-ink optimized falling block puzzle with ghosting prevention, high scores, pause/resume, and reset modals. |
| **Wordle** | 5-letter word deduction game with interactive on-screen keyboard, letter hints, and win statistics. |
| **Sudoku** | Infinite procedural 9×9 logic puzzles with difficulty tiers, conflict highlights, and board generator. |
| **2048** | Classic 4×4 sliding tile puzzle with persistent state saving, smooth input debouncing, and high scores. |
| **Holy Rosary** | Interactive step-by-step prayer companion with high-resolution crucifix pixel art and bead tracking. |
| **Dice Simulator** | Multi-dice roller (d4, d6, d8, d10, d12, d20, d100, coin toss) with rolling animation and history log. |
| **Life Counter** | Multiplayer tabletop life tracker for *Magic: The Gathering* and card games with commander damage. |

### ⚙️ System & Firmware Enhancements
* **Front Button Navigation:** Unified physical button navigation across the App Store, Applications menu, and games (physical Up/Down/Select/Back controls).
* **Dedicated Over-The-Air (OTA) Updates:** Owner-controlled Crosscub-only release validation; no Crosscub update is published yet. Upstream product images are never selected by OTA. Manual SD recovery remains available.
* **Low-Latency E-Paper Engine:** Custom partial refresh scheduling and waveform management to minimize ghosting during interactive gameplay.

---

## Screenshot Gallery 📸

<table>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/dashboard.png" alt="Desk Stand: Ambient E-Ink Desk Station" /><br/>
      <em>Desk Stand: Portrait Flip Clock & Calendar Station</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/dashboard_landscape.png" alt="Desk Stand: Landscape Mode" /><br/>
      <em>Desk Stand: 800×480 Side-by-Side Landscape Mode</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/stopwatch.png" alt="Stopwatch: Pixel Cartoon Character with Lap Splits" /><br/>
      <em>Stopwatch: Cartoon Character, LCD Timer & Splits</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/hourglass.png" alt="Hourglass: Dynamic Sand Funnel Timer" /><br/>
      <em>Hourglass: Dynamic Sand Funnel Crater & Stream</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/solitaire.png" alt="Solitaire: Klondike Card Game" /><br/>
      <em>Solitaire: 7-Column Klondike Card Game</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/watertracker.png" alt="Water Tracker: Daily Hydration Companion" /><br/>
      <em>Water Tracker: Pixel Cup & Daily Hydration</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/app_store_catalog.png" alt="On-Device App Store Catalog" /><br/>
      <em>On-Device App Store: Wireless Community Catalog</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/app_store_detail.png" alt="App Store Hero Detail View" /><br/>
      <em>App Store: Detail View with 1-Click Install/Update</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/applications_menu.png" alt="Applications Menu Launcher" /><br/>
      <em>Applications Menu with High-Contrast Icons</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/codex_combat.png" alt="Codex: Ink & Iron Deckbuilder RPG" /><br/>
      <em>Codex: Ink & Iron (Roguelike Card Deckbuilder)</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/blackjack.png" alt="Blackjack with Split Hands" /><br/>
      <em>Blackjack: Split Pairs, Double Down & Card Sprites</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/daily_office.png" alt="Divine Worship Daily Office" /><br/>
      <em>Divine Worship: Liturgical Daily Office & Psalter</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/wordle.png" alt="Wordle with On-Screen Keyboard" /><br/>
      <em>Wordle: E-Paper Keyboard & Word Deduction</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/sudoku.png" alt="Sudoku Logic Puzzle" /><br/>
      <em>Sudoku: Procedural Generator & Contrast Grid</em>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="./docs/images/apps/blockdrop.png" alt="Block Drop Falling Block Game" /><br/>
      <em>Block Drop: Fast E-Ink Partial Refresh Gameplay</em>
    </td>
    <td align="center" width="50%">
      <img src="./docs/images/apps/rosary.png" alt="Holy Rosary Prayer Companion" /><br/>
      <em>Holy Rosary: Bead Tracking & Crucifix Pixel Art</em>
    </td>
  </tr>
</table>

---

## Developer Guide: Building Your Own Apps 🛠️

Crosscub makes creating e-paper applications accessible to everyone. You do not need an ESP-IDF toolchain or C++ knowledge; any text editor is all you need to build standalone applications in **Lua 5.4**.

### 1. Application Anatomy
Every application is a folder placed inside `apps/<app_id>/` (or on your device in `/.crosssmudge/applications/<app_id>/`):

```text
my_app/
├── manifest.json       # Metadata: id, name, version, author, description
├── main.lua            # Entry point script with lifecycle callbacks
└── icon.raw            # (Optional) 32x32 1-bit monochrome icon (128 bytes)
```

#### `manifest.json`
```json
{
  "id": "my_app",
  "name": "My Application",
  "version": "1.0.0",
  "author": "YourGitHubUsername",
  "description": "A clean, functional e-paper utility for Crosscub.",
  "entry": "main.lua"
}
```

### 2. Quickstart Code Example (`main.lua`)
```lua
local count = 0
local width, height = 480, 800

function on_init()
    width, height = smudge.get_bounds()
    -- Load persisted state from device SD card
    count = tonumber(smudge.load("counter", "0")) or 0
end

function on_draw()
    smudge.clear()
    smudge.header("Counter App", string.format("Count: %d", count))

    -- Render centered card box and text
    local boxW, boxH = 260, 160
    local boxX = math.floor((width - boxW) / 2)
    local boxY = 280
    smudge.rounded_rect(boxX, boxY, boxW, boxH, 8, false, 2)
    smudge.centered_text(boxY + 50, tostring(count), "ui12", "bold", true)
    smudge.centered_text(boxY + 100, "Press Select to increment", "small", "regular", true)

    -- Bottom button hint labels
    smudge.button_hints("Exit", "+ 1", "Reset", "")
end

function on_button(btn, pressed)
    if not pressed then return end

    if btn == "back" then
        smudge.save("counter", tostring(count))
        smudge.exit()
    elseif btn == "confirm" then
        count = count + 1
        smudge.request_update()
    elseif btn == "left" then
        count = 0
        smudge.request_update()
    end
end

function on_exit()
    smudge.save("counter", tostring(count))
end
```

### 3. The `smudge.*` API Overview
The sandboxed runtime provides a focused set of graphics, layout, input, and persistence APIs:
- **Display & Drawing:** `smudge.clear()`, `smudge.request_update()`, `smudge.rect()`, `smudge.rounded_rect()`, `smudge.circle()`, `smudge.line()`, `smudge.draw_bitmap_1bit()`.
- **Typography & UI:** `smudge.text()`, `smudge.centered_text()`, `smudge.wrapped_text()`, `smudge.header()`, `smudge.button_hints()`, `smudge.modal()`.
- **System & State:** `smudge.load(key, default)`, `smudge.save(key, val)`, `smudge.get_bounds()`, `smudge.time()`, `smudge.exit()`.

👉 **[Read the Full Application Developer Guide (500+ lines of documentation & API specifications)](./apps/README.md)**

### 4. Sticky Production Budgets and Regression Testing
Sticky's PSRAM-backed Lua runtime uses a **2 MB per-app Lua heap** by default. The explicitly declared Browser capability uses a **4 MB Lua heap**, subject to the production allocator's PSRAM reserve/admission checks (`LuaRunner.cpp` and `lib/LuaBrowser/BrowserMemory.h`). These are per-app limits, not a promise that every allocation succeeds. Do not change runtime budgets to satisfy a fixture.

Run the current dependency-free product contracts and the relevant preservation group:

```bash
python3 test/crosscub/test_product.py
python3 test/crosscub/run_preservation.py --group apps --output /path/to/new-evidence/apps
# Full host routine: set existing read-only ARDUINOJSON_INCLUDE_DIR,
# CP_FIXTURES and ESP_IDF_SOURCE_ROOT first; use a Python with Pillow:
python3 test/crosscub/run_all.py --output /path/to/new-evidence/full
```

`SMUDGE_X3_CONSTRAINTS=1` remains an **optional stress** compatibility alias for a 75 KB simulator Lua heap. It is not Sticky's production acceptance ceiling. Host fixtures do not replace target compilation or physical touch/e-ink, radio, SD and sleep/wake qualification.

---

## How to Submit Your App to the Official App Store 🌐

Want to share your creation with all Crosscub users worldwide?

1. **Verify Your App:** Ensure your app lives in `apps/<your_app_id>/`, passes the relevant Sticky production-budget and preservation tests above, and retains existing IDs and saves.
2. **Add to `apps/catalog.json`:** Add an entry for your application to [`apps/catalog.json`](./apps/catalog.json):
   ```json
   {
     "id": "your_app_id",
     "name": "Your App Name",
     "version": "1.0.0",
     "author": "YourGitHubHandle",
     "description": "One-line summary of what your application does.",
     "files": [
       "manifest.json",
       "main.lua",
       "icon.raw"
     ]
   }
   ```
3. **Prepare a Pull Request:** Open a PR against `feat/crosscub-sticky-only` in [tobiasweissert/CrossSmudge](https://github.com/tobiasweissert/CrossSmudge) for source review. Merging this feature branch does not publish an app.
4. **Separate Live Publication:** Only an explicitly authorized per-app composition/publication into `integration/sticky-test-all` updates the configured live App Store. Preserve the live catalog's 41 entries and unrelated metadata; never replace it with this branch's 37-source-app catalog. Firmware store routing remains unchanged.

---

## Core Reading Features & Typography (CrossInk Heritage) 📖

Crosscub preserves the reader improvements and typography enhancements originally created in **[CrossSmudge](https://github.com/Mumfee/CrossSmudge)** and **[CrossInk](https://github.com/uxjulia/crossink)**:

<table>
  <tr>
    <td align="center">
      <img src="./docs/images/bitter-small-15-margin.jpg" alt="Font: Bitter, Size: 12 pt, Margin: 15" /><br/>
      <em>Font: Bitter, Size: 12 pt, Margin: 15</em>
    </td>
    <td align="center">
      <img src="./docs/images/reading-stats.jpg" alt="Reading Stats with custom front button mapping shown" /><br/>
      <em>Reading Stats with customizable front button mapping</em>
    </td>
  </tr>
</table>

### Reading & Typography Highlights
- **Curated E-Paper Typefaces:** Default body fonts replaced with [Lexend Deca](https://fonts.google.com/specimen/Lexend+Deca) and [Bitter](https://fonts.google.com/specimen/Bitter) for maximum legibility and reduced ghosting. UI rendered with [Inter](https://fonts.google.com/specimen/Inter).
- **Music & Supplemental Glyphs:** Comprehensive glyph set supporting music notation, selected Cyrillic characters, and *Project Hail Mary* CJK fallback ranges.
- **Reading Statistics:** Total books finished, reading time, sessions, pages turned, and reading speed metrics, viewable on-demand or as a customized sleep screen.
- **Enhanced EPUB Rendering:** Thicker underlines, `<hr>` section breaks, redaction markup rendering, and improved simple tables.
- **Focus Reading & Guide Dots:** Optional visual reading aids for tracking lines and reducing eye fatigue.
- **Custom Button Actions:** Complete mapping for front buttons, side switches, short power clicks, and long-press shortcuts.
- **Finished Book Tracking:** Automatically prompts when reaching 99% of a book, tracks completion stats, and offers to organize completed titles into a "Read" folder.
- **Cross-Device Sync:** Synchronize reading progress and all-time reading stats wirelessly between two devices over local Wi-Fi.

---

## Installation & Updating

Crosscub targets **reTerminal Sticky only**. No Crosscub firmware release has
been published yet. Source is on `feat/crosscub-sticky-only` in
[tobiasweissert/CrossSmudge](https://github.com/tobiasweissert/CrossSmudge).
The repository name is retained; it is not the active product name.

For a verified application image, keep an SD backup and use **Settings → System
→ SD Card Firmware Update**, select the `.bin`, and confirm. This is an
application update, not a factory image. Do not format SD/NVS: existing
`.crosspoint`, `.crosssmudge` and `.smudge` settings, reading progress, apps and
saves stay in place. Custom boot/sleep images and preferences remain supported.

OTA remains available in Settings. It accepts only the owner's `crosscub-v…`
release tags, `crosscub-sticky-v….bin` assets, pinned SHA-256 and Crosscub
application identity. It never selects upstream product firmware. Until an
owner release exists, the update screen reports no Crosscub update published.
See [installation](docs/installation.md) before changing firmware.

---

## Development & Building from Source 💻

### Prerequisites
- [PlatformIO Core](https://platformio.org/install/cli) or PlatformIO IDE extension.
- Git with submodule support.

### Clone and Initialize
```bash
git clone --branch feat/crosscub-sticky-only https://github.com/tobiasweissert/CrossSmudge.git
cd crosssmudge
git submodule update --init --recursive
```

### Build Targets

| Command | Target Device / Platform |
| :--- | :--- |
| `pio run -e sticky` | Seeed Studio reTerminal Sticky (ESP32-S3, touch, PSRAM) |
| `pio run -e simulator` | Native desktop simulator (SDL2) |

### Flash via USB
```bash
pio run -e sticky --target upload
```

### Nix / NixOS
A Nix flake and shell are provided:
```bash
nix develop -f nix
# or
nix-shell nix
```

---

## Repository Structure 📂

- `src/` — Main application logic, activity manager, UI themes, and screen activities (Reader, Home, App Store, Settings, Network).
- `apps/` — Lua application ecosystem, manifests, icons, and official community catalog (`catalog.json`).
- `lib/` — Core firmware libraries (EPUB parser/layout, fonts, i18n, filesystem helpers, HAL abstractions).
- `freeink-sdk/` — Nested hardware SDK submodule providing low-level display, power, and board drivers.
- `web/` — Web portal sources (HTML templates, CSS styles, JavaScript app manager); compiled into C++ headers by `scripts/build_web.py`.
- `docs/` — System architecture documentation, user guides, controls, and developer tutorials.
- `scripts/` — Tooling for icon generation, translation builds, web packaging, and release automation.
- `test/` — Unit test suites and native simulator fixtures.

---

## Acknowledgments & Upstream Credits 🤝

Crosscub stands on the shoulders of giants:
- **[CrossSmudge](https://github.com/Mumfee/CrossSmudge)** and **[CrossInk](https://github.com/uxjulia/crossink)** by **Julia (@uxjulia)** — For the reader typography, reading statistics, focus reading modes, and design sensibilities that form the core reading experience.
- **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)** — For the original open-source ESP32 e-reader architecture and EPUB engine.
- **[FreeInk SDK](https://freeink.org/)** — For the underlying hardware drivers and e-paper display abstractions.

---

## License

Crosscub's root [`LICENSE`](./LICENSE) is **MIT**. Component-specific licenses, notices and attribution obligations remain in force for inherited and bundled components; consult their license files as well. This description does not relicense any component.
