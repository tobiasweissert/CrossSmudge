> **Experimental migration candidate — not release-ready.** Crosscub remains Sticky-only.
> This branch uses pinned CrossPoint develop directly, selectively adopts CrossInk,
> and preserves Crosscub behavior. Stable Crosscub **1.6.5.5 is unchanged**.
> Source/host checks do not establish a complete firmware build or device acceptance.
> See [the feature and readiness ledger](../../SCOPE.md).

---
title: Getting Started
parent: Development
nav_order: 1
---

# Getting Started

This guide helps you build and run Crosscub locally.

## Prerequisites

- [pioarduino Core](https://github.com/pioarduino/platformio-core) (`pio`) or VS Code + [pioarduino IDE](https://github.com/pioarduino/pioarduino-vscode-ide), using the pinned core below
- Python 3.13 (matches CI)
- `clang-format` 21+ in your `PATH` (CI uses clang-format 21)
- USB-C cable
- A device matching the build profile for hardware testing

If `./bin/clang-format-fix` fails with either of these errors, install clang-format 21:

- `clang-format: No such file or directory`
- `.clang-format: error: unknown key 'AlignFunctionDeclarations'`

Examples:

```sh
# Debian/Ubuntu (try this first)
sudo apt-get update && sudo apt-get install -y clang-format-21

# If the package is unavailable, add LLVM apt repo and retry
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 21
sudo apt-get update
sudo apt-get install -y clang-format-21

# macOS (Homebrew)
brew install clang-format
```

Then verify:

```sh
clang-format-21 --version
```

The reported major version must be 21 or newer.

## Clone and initialize

```sh
git clone --recursive https://github.com/tobiasweissert/Crosscub.git
cd Crosscub
```

If you already cloned without submodules:

```sh
git submodule update --init --recursive
```

## Build

With an existing compilation setup, run the selected profile directly:

```sh
pio run -e sticky
```

`pio run` selects standard Sticky. Shared SDK board profiles do not add Crosscub targets.
Keep the installed tools, packages, and build caches. The setup commands below
apply only to a new environment or an identified missing or incompatible
dependency; they are not steps to repeat before each build.

## First-time toolchain setup

Use the pioarduino Core revision installed by
[CI](../../.github/workflows/ci.yml), together with the platform pinned in
`platformio.ini`. On Linux or macOS, install the core in the ignored local
virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade 'https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip'
pio --version
```

On Windows, create the environment with `py -3.13 -m venv .venv` and activate
`.venv/Scripts/activate` in Git Bash, or `.venv\Scripts\Activate.ps1` in PowerShell.
Use this core in the IDE too; installing the IDE alone does not verify its core
version. CI also lists the Python dependencies needed for a fresh platform setup.

Profiles with `custom_sdkconfig`, including `default` and `sticky`, build a
nested core environment. Pin that core too before the first build, as CI does:

```sh
python -m pip install uv
pio pkg install -e sticky
uv pip install --python ~/.platformio/penv/bin/python 'pioarduino==6.1.19'
```

Crosscub uses the standard `sticky` profile. The nested Python path above uses
the default Linux/macOS PlatformIO directory; on Windows use
`~/.platformio/penv/Scripts/python.exe`, or the configured core directory.
Keep both core pins aligned with CI when updating the toolchain. After setup,
use the [normal build command](#build).

## Flash

```sh
pio run -e sticky --target upload
```

## Validation

```sh
./bin/clang-format-fix
pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high
pio run
```

Development builds apply the Git version only to sources that use
`CROSSPOINT_VERSION`, so a branch/SHA change does not invalidate unrelated objects.

## What to read next

- [Architecture Overview](./architecture.md)
- [Testing and Debugging](./testing-debugging.md)

## Candidate CI and provenance

PRs and explicit manual CI dispatch compile only `sticky`, with no release write
permission and no publication. Core **6.1.19** is pinned in both the outer Python
and `~/.platformio/penv` (nested custom-sdkconfig build); platform **55.03.311** and
SDK **634a3f8c067a3714a6a7c622015138af096ab5d2** must match. Package/framework/toolchain
versions are measured, not inferred from the platform label. Retain BIN, ELF,
build log, source/SDK/toolchain manifests and strict image-validation result in a
flat Actions artifact immediately after the build, before stats or another environment.
Artifacts are access-controlled by the repository's Actions policy, not public release
assets; do not run this on a public fork when confidential retention is required.
This prepared workflow has not been dispatched or built by this reconciliation.
