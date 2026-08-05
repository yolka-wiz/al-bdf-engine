#!/usr/bin/env bash
# Install albdf git hooks (sets core.hooksPath to .githooks).
#
#   scripts/install-hooks.sh
#
# After this, `git commit` runs .githooks/pre-commit automatically:
#   - regenerates + stages REPO_MAP.md
#   - gates staged markdown structure (except README.md / vendored skills)
#
# The hook file itself is committed, so every clone can install the same hooks.
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

chmod +x "$REPO_DIR/.githooks/pre-commit" 2>/dev/null || true
git -C "$REPO_DIR" config core.hooksPath .githooks
echo "hooks installed: core.hooksPath = $(git -C "$REPO_DIR" config core.hooksPath)"
echo "next 'git commit' will run .githooks/pre-commit"
