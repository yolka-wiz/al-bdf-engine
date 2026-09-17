#!/usr/bin/env python3
"""Generate REPO_MAP.md — a deterministic orientation index for AI agents.

The pre-commit hook (.githooks/pre-commit) runs this and stages the result,
so REPO_MAP.md always reflects the committed tree. It is deliberately
DETERMINISTIC: no timestamps, no absolute paths, sorted output — running it
twice on the same tree produces byte-identical output (repo determinism law).

Run manually:  python3 scripts/gen-repo-map.py
Output:        REPO_MAP.md (repo root)
"""
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "REPO_MAP.md")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def git(*args):
    try:
        return subprocess.run(
            ["git", "-C", REPO, *args],
            capture_output=True, text=True, timeout=10, check=False,
        ).stdout.strip()
    except Exception:
        return ""


def first_paragraph(path):
    """Return the first meaningful paragraph (H1 + following non-heading text)."""
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError:
        return "", ""
    title = ""
    body = []
    for line in lines:
        s = line.strip()
        if s.startswith("# "):
            title = s[2:].strip()
        elif s.startswith("#") and title:
            continue  # later headings
        elif s and not s.startswith("```"):
            body.append(s)
        if title and len(body) >= 3:
            break
    # Trim to one sentence-ish
    text = " ".join(body).strip()
    if len(text) > 180:
        cut = text[:180]
        # break at last space
        text = cut[: cut.rfind(" ")] + "…"
    return title, text


def walk_md(root, skip_dirs):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        for f in sorted(filenames):
            if f.endswith(".md"):
                out.append(os.path.relpath(os.path.join(dirpath, f), root))
    return sorted(out)


def heading(text, level):
    return f"{'#' * level} {text}"


# ---------------------------------------------------------------------------
# Section builders
# ---------------------------------------------------------------------------

def section_header():
    lines = [
        "# REPO_MAP.md — albdf repository orientation index",
        "",
        "> **Purpose:** one-page orientation for humans and AI agents: what this",
        "> repo is, how to build/test it, and where every document lives. This",
        "> file is **auto-generated** by `scripts/gen-repo-map.py` from the",
        "> pre-commit hook (`.githooks/pre-commit`). Do not edit by hand; the",
        "> next commit regenerates it. Deterministic: no timestamps, sorted.",
        "",
        "> **Binding contracts:** read `AGENTS.md` first — it overrides general",
        "> coding habits. The tracking DB (`db/albdf.db`, gitignored) is the",
        "> source of truth for component/task status; rebuild with",
        "> `python3 scripts/db.py init && python3 db/seed.py` on a fresh clone.",
        "",
    ]
    return lines


def section_quickstart():
    return [
        heading("Quick start (build & test)", 2),
        "",
        "```bash",
        "source ~/.bashrc",
        "cd src",
        "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \\",
        "    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \\\\",
        "    -DVCPKG_OVERLAY_PORTS=$PWD/vcpkg/overlays -DALBDF_BUILD_TESTS=ON",
        "cmake --build build -j$(nproc)",
        "QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure",
        "QT_QPA_PLATFORM=offscreen bash src/tests/smoke.sh src/build/bin/albdf src/tests/fixtures",
        "bash ci/run-ci.sh   # full gate: Release + ctest + ASAN/UBSAN + clang-format",
        "```",
        "",
        "> Build dir is `src/build` (repo root has no CMakeLists). vcpkg manifest",
        "> deps: `$VCPKG_ROOT` (vcpkg manifest mode). Aliyun mirrors everywhere.",
        "> Headless: `QT_QPA_PLATFORM=offscreen` is mandatory for all Qt runs.",
        "",
    ]


def section_dirs():
    # Hand-curated map of the top-level layout (from README/AGENTS). Auto-checked
    # against the live tree: dirs listed but missing are flagged; extra dirs noted.
    known = {
        "agents/": "Per-role agent contracts (core, cli, rtl, test, research, orchestrator)",
        "ci/": "CI gate: run-ci.sh (build + ctest + ASAN/UBSAN + clang-format)",
        "db/": "Tracking DB: schema.sql + seed.py committed; albdf.db gitignored (db.py CLI)",
        "docs/": "PROBLEMS.md, RELEASES.md, ADRs (decisions/), research notes, man page, coding standard",
        "plans/": "PLAN.md active roadmap (v3) + archive/ of completed execution plans",
        "scripts/": "Tooling: db.py (tracking DB), gen-repo-map.py (this file), install-hooks.sh",
        "skills/": "Vendored Qt Company agent skills (qt-cmake-project, qt-cpp-docs, qt-cpp-review)",
        "src/": "The fork: Pdf4QtLibCore (engine) + PdfTool (CLI) + UnitTests + tests",
        ".githooks/": "Git hooks: pre-commit regenerates REPO_MAP.md + markdown structure gate",
    }
    lines = [heading("Top-level directory map", 2), "", "| Path | Purpose |", "|---|---|"]
    existing = sorted(
        d for d in os.listdir(REPO)
        if os.path.isdir(os.path.join(REPO, d)) and not d.startswith(".") and d != "src"
    )
    # Always show src first-ish? Keep sorted but ensure known order stable.
    for d in sorted(known):
        status = "" if os.path.isdir(os.path.join(REPO, d)) else " *(MISSING)*"
        lines.append(f"| `{d}` | {known[d]}{status} |")
    for d in existing:
        if d not in known:
            lines.append(f"| `{d}/` | *(unmapped — add to scripts/gen-repo-map.py)* |")
    lines.append("")
    return lines


def section_docs_index():
    skip = {".git", "build", "build-asan", "skills"}
    docs = walk_md(REPO, skip)
    lines = [heading("Document index (markdown, except README.md)", 2), ""]
    lines.append("| Document | Title | One-liner |")
    lines.append("|---|---|---|")
    for rel in docs:
        if rel == "README.md":
            continue
        title, one = first_paragraph(os.path.join(REPO, rel))
        if not title:
            title = rel
        one = one.replace("|", "\\|") if one else "—"
        lines.append(f"| `{rel}` | {title} | {one} |")
    lines.append("")
    return lines


def section_src_map():
    # Key source entry points (hand-curated, verified against tree).
    known = {
        "src/PdfTool/main.cpp": "CLI entry point (albdf binary)",
        "src/PdfTool/pdftooladdtext.cpp": "add-text command (LTR + RTL)",
        "src/PdfTool/pdftoolsearchtext.cpp": "search-text command (RTL-aware)",
        "src/PdfTool/pdftooldeleteobject.cpp": "delete-object command",
        "src/PdfTool/pdftoolformlist.cpp": "form-list command (AcroForm)",
        "src/PdfTool/pdftoolformfill.cpp": "form-fill command",
        "src/PdfTool/pdftoolsign.cpp": "sign command (PKCS#7 / PAdES)",
        "src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp": "RTL write pipeline: FriBidi + HarfBuzz shaping + Type0 embed + Ts mark offsets",
        "src/Pdf4QtLibCore/sources/pdfrtltextnormalizer.cpp": "RTL normalization (tashkeel, presentation forms, digits)",
        "src/Pdf4QtLibCore/sources/pdftextsearchengine.cpp": "search engine: joined visual string + span mapping (S#1)",
        "src/Pdf4QtLibCore/sources/pdfpagecontentprocessor.cpp": "content stream processor (render + flow)",
        "src/Pdf4QtLibCore/sources/pdfdocumenttextflow.cpp": "text flow factory (fetch-text / search input)",
        "src/Pdf4QtLibCore/sources/pdftextlayout.cpp": "text layout + phantom-space heuristic (P3 guard)",
    }
    lines = [heading("Key source files", 2), "", "| File | Role |", "|---|---|"]
    for f, role in sorted(known.items()):
        exists = os.path.exists(os.path.join(REPO, f))
        mark = "" if exists else " *(MISSING)*"
        lines.append(f"| `{f}` | {role}{mark} |")
    lines.append("")
    return lines


def section_db_status():
    lines = [heading("Tracking DB status", 2), ""]
    db = os.path.join(REPO, "db", "albdf.db")
    if not os.path.exists(db):
        lines.append("_DB not present (gitignored). Rebuild:_")
        lines.append("")
        lines.append("```bash")
        lines.append("python3 scripts/db.py init && python3 db/seed.py")
        lines.append("```")
        lines.append("")
        return lines
    try:
        status = subprocess.run(
            [sys.executable, os.path.join(REPO, "scripts", "db.py"), "status"],
            capture_output=True, text=True, timeout=15,
        ).stdout
    except Exception:
        status = ""
    lines.append("```text")
    lines.append(status.strip()[:2000] if status else "(db.py status unavailable)")
    lines.append("```")
    lines.append("")
    return lines


def section_rules():
    return [
        heading("Contributing rules (summary)", 2),
        "",
        "Full rules: `CONTRIBUTING.md`. The five binding rules:",
        "",
        "1. **Branch per task**; commit each major step (Conventional Commits).",
        "2. **Update docs** about what you change, as you change it.",
        "3. **No push to main until fully tested** (`ci/run-ci.sh` green).",
        "4. **Update PROBLEMS.md** for any problem your change fixes/creates.",
        "5. **No unnecessary files, secrets, or data** in the repo.",
        "",
    ]


def main():
    sections = [
        section_header(),
        section_quickstart(),
        section_dirs(),
        section_docs_index(),
        section_src_map(),
        section_db_status(),
        section_rules(),
    ]
    body = "\n".join("\n".join(s) for s in sections)
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write(body)
    print(f"REPO_MAP.md written: {len(body)} bytes -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
