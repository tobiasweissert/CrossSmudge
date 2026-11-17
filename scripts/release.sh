#!/usr/bin/env bash
# Usage: ./scripts/release.sh 1.2.7
# Bumps [crosssmudge] version in platformio.ini, commits, and creates a Crosscub product tag.
set -euo pipefail

VERSION="${1:-}"
if [[ -z "$VERSION" ]]; then
  echo "Usage: $0 <version>  (e.g. 1.2.7)" >&2
  exit 1
fi

sed -i '' "/^\\[crosssmudge\\]/,/^\\[/ s/^version = .*/version = $VERSION/" platformio.ini
git add platformio.ini
git commit -m "Update Crosscub version to $VERSION"
git tag "crosscub-v$VERSION"
echo "Tagged crosscub-v$VERSION — push with: git push && git push origin crosscub-v$VERSION"
