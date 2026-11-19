#!/usr/bin/env bash
# Usage: scripts/release.sh 1.6.5.4 [--write-version] [--publish]
# Plan only by default. --write-version updates both INI fields portably.
# --publish prints the explicitly approved stable publication command; it does
# not run it. No implicit commit, tag, push, build or upstream-origin assumption.
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$SCRIPT_DIR/prepare_ota_release.py" "$@"
