#!/usr/bin/env python3
"""Markdown structure gate for albdf docs.

Enforces the repo rule: every markdown file EXCEPT README.md must be
structured — an H1 title, at least 2 headings, and a body long enough to be
useful ("not minimal but close"). Logical structure is a human judgment; this
gate catches the mechanical failures (no title, single heading, stub file).

Exempt:
  - README.md (repo convention: README is the entry point, not gated)
  - skills/** (vendored Qt Company skills — cherry-pick hygiene, same as the
    CI format gate's upstream exclusions)

Run:
  python3 scripts/check-markdown-structure.py [files...]
  (no args = scan the whole repo; the pre-commit hook passes staged .md files)

Exit: 0 = pass, 1 = failures found.
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

MIN_BODY_LINES = 12
MIN_BODY_LINES_ADR = 8
MIN_HEADINGS = 2

# Vendored/exempt paths (relative to repo root, prefix match).
EXEMPT_PREFIXES = ("skills/",)
EXEMPT_NAMES = {"README.md"}


def is_exempt(rel):
    if os.path.basename(rel) in EXEMPT_NAMES:
        return True
    for p in EXEMPT_PREFIXES:
        if rel.startswith(p):
            return True
    return False


def check_file(path, rel):
    # ADRs are intentionally terse (context/decision/consequences); relax the
    # body-length floor for them, but still require title + sections.
    is_adr = os.path.basename(path).startswith("000") and "/decisions/" in path
    min_body = MIN_BODY_LINES_ADR if is_adr else MIN_BODY_LINES

    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    lines = text.splitlines()
    stripped = [l.strip() for l in lines]

    h1 = [l for l in stripped if l.startswith("# ")]
    headings = [l for l in stripped if l.startswith("#")]
    code_fence = False
    body_lines = 0
    for l in stripped:
        if l.startswith("```"):
            code_fence = not code_fence
            continue
        if not code_fence and l and not l.startswith("#"):
            body_lines += 1

    errors = []
    if not h1:
        errors.append("missing H1 title (# ...)")
    if len(headings) < MIN_HEADINGS:
        errors.append(f"only {len(headings)} heading(s); need >= {MIN_HEADINGS} (title + sections)")
    if body_lines < min_body:
        errors.append(f"only {body_lines} non-heading body line(s); need >= {min_body}")

    # Fenced code fences must balance (catches truncated ``` blocks).
    if text.count("```") % 2 != 0:
        errors.append("unbalanced ``` code fences")

    return errors


def main():
    args = sys.argv[1:]
    if args:
        files = []
        for a in args:
            p = os.path.abspath(a)
            if os.path.isfile(p):
                files.append((p, os.path.relpath(p, REPO)))
    else:
        files = []
        for root, dirs, fnames in os.walk(REPO):
            dirs[:] = [d for d in dirs if d not in (".git", "build", "build-asan")]
            for f in sorted(fnames):
                if f.endswith(".md"):
                    p = os.path.join(root, f)
                    files.append((p, os.path.relpath(p, REPO)))

    failed = 0
    checked = 0
    for path, rel in files:
        if is_exempt(rel):
            continue
        checked += 1
        errors = check_file(path, rel)
        if errors:
            failed += 1
            print(f"FAIL {rel}")
            for e in errors:
                print(f"     - {e}")

    print(f"markdown structure gate: {checked} files checked, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
