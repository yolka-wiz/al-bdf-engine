#!/usr/bin/env python3
"""Seed the albdf tracking DB with the current known state (2026-09-17).

Run after `python3 scripts/db.py init`. Idempotent-ish: components are
INSERT OR IGNORE, tasks/decisions/questions are inserted fresh (run once).
The evidence_ref on each task is the commit sha that closed it.
"""
import os, sqlite3, sys

DB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "db", "albdf.db")

COMPONENTS = [
    # (id, name, status, owner_agent, description, source_dir)
    ("fork-base", "PDF4QT fork: Pdf4QtLibCore + PdfTool", "done", "core-agent",
     "Vendored fork of MIT PDF4QT; upstream remote tracked; GUI apps stripped. Everything else builds on this.",
     "src/"),
    ("cli", "CLI surface (albdf binary)", "done", "cli-agent",
     "Extend upstream PdfTool into the albdf CLI with delete-object / add-text / form / sign commands; deterministic output; existing ~30 commands kept.",
     "src/PdfTool/"),
    ("text-recognition", "Text recognition as objects", "done", "core-agent",
     "PDFDocumentTextFlow::Item with bbox + char boxes already in PDF4QT core; exposed via CLI (fetch-text / recognize-text).",
     "src/Pdf4QtLibCore/"),
    ("object-deletion", "Whole-object deletion (text runs, images, elements)", "done", "core-agent",
     "Wire PDFDocumentTextFlowEditor::removeItem + PDFPageContentEditorProcessor to CLI delete-object. Image refcount/nesting edge cases.",
     "src/Pdf4QtLibCore/"),
    ("add-text", "Add text (LTR + RTL) via CLI", "done", "core-agent",
     "CLI add-text with font embedding + positioning + write-back; RTL via FriBidi + HarfBuzz pipeline.",
     "src/Pdf4QtLibCore/"),
    ("rtl-writer", "RTL write pipeline (Arabic/Persian/Hebrew)", "done", "rtl-agent",
     "GREENFIELD: FriBidi bidi + HarfBuzz shaping + Type0/Identity-H embed + ToUnicode CMap + ActualText. The differentiator.",
     "src/Pdf4QtLibCore/"),
    ("rtl-search", "RTL-aware search", "done", "rtl-agent",
     "Normalize + bidi inversion on top of existing text flow extraction. No OSS incumbent does this.",
     "src/Pdf4QtLibCore/"),
    ("forms-signatures", "Forms (AcroForm) + digital signatures", "done", "core-agent",
     "form-list / form-fill / sign (PKCS#7 detached, PAdES byte-range) / verify-signatures. ADR-0006.",
     "src/PdfTool/"),
    ("tests", "Test infrastructure: unit + golden + CLI", "done", "test-agent",
     "Deterministic golden-image tests, RTL corpus (ZWNJ/lam-alef/tashkeel/digits), offscreen CI.",
     "src/tests/"),
    ("research", "Research stream (Rosetta)", "done", "rosetta",
     "Deep dives: PDF4QT API surface, RTL reference impls, skill sourcing, AGENTS.md best practice.",
     "docs/research/"),
]

TASKS = [
    # (component_id, title, priority, estimate, assignee, status, notes, evidence_ref)
    ("fork-base", "Vendor PDF4QT fork into src/, strip GUI apps (Viewer/Editor/PageMaster/Diff/LaunchPad)", 1, "2-4d", "core-agent", "done",
     "Keep Pdf4QtLibCore + PdfTool + UnitTests; add upstream remote for cherry-picking.", "a52c18c"),
    ("fork-base", "Verify albdf headless run: QT_QPA_PLATFORM=offscreen build + fetch-text smoke test", 1, "1d", "core-agent", "done",
     "main() uses QGuiApplication; confirm offscreen works on bare container.", "9ca9c38"),
    ("cli", "Add delete-object CLI command (wire TextFlowEditor::removeItem + write-back)", 1, "2-4d", "core-agent", "done",
     "Reads page + object id/rect, removes, writes incremental save.", "f354005"),
    ("cli", "Add add-text CLI command (LTR)", 2, "3-6d", "core-agent", "done",
     "Text + position + font; reuse PDFTextLayoutGenerator; needs font embedding path.", "327061b"),
    ("object-deletion", "Image XObject reference-counting + Form XObject nesting for deletion", 2, "3-5d", "core-agent", "done",
     "Don't orphan shared resources; recurse into nested forms; inline images BI..EI.", "f354005"),
    ("rtl-writer", "Add HarfBuzz + FriBidi deps to build (MIT/LGPL)", 1, "1-2d", "rtl-agent", "done", "", "fe016c4"),
    ("rtl-writer", "RTL add-text: bidi runs + HarfBuzz shaping -> visual-order Tj", 1, "4-6w", "rtl-agent", "done",
     "Absolute-position Tm per run; PDF has no kerning, bake advances.", "fe016c4"),
    ("rtl-writer", "ToUnicode CMap from HarfBuzz clusters + ActualText marked content", 1, "1-2w", "rtl-agent", "done",
     "Subset GIDs pitfall; map lam-alef to 2 codepoints.", "fe016c4"),
    ("rtl-search", "Normalization: tashkeel, presentation forms, lam-alef, Persian/Arabic unification, digits, ZWNJ", 2, "2-3w", "rtl-agent", "done", "", "aa92bee"),
    ("rtl-search", "Bidi inversion of extracted visual-order text for search matching", 2, "2-3w", "rtl-agent", "done",
     "Pure-RTL = straight reversal; mixed lines need level-based run split.", "aa92bee"),
    ("rtl-search", "S#1: cross-item phrase search via joined visual string + span mapping", 2, "2-3d", "rtl-agent", "done",
     "Joined per-page visual string (line-cluster + x-sort, word-gap separator); searchFlow() test seam; CLI item col first+last.", "6b26d14"),
    ("tests", "Golden-image harness (deterministic render diff)", 1, "1-2w", "test-agent", "done", "", "8b9d5bd"),
    ("tests", "RTL corpus: Persian/Arabic/Hebrew fixtures incl. ZWNJ, lam-alef, tashkeel, digits", 2, "1-2w", "test-agent", "done", "", "aa92bee"),
    ("tests", "CI: offscreen ctest + ASAN job", 2, "1w", "test-agent", "done", "", "dc45fcb"),
    ("forms-signatures", "form-list: enumerate AcroForm fields (name, type, value, page, rect)", 1, "1-3d", "core-agent", "done", "", "3b3e563"),
    ("forms-signatures", "form-fill: set field values + regenerate appearances, write new doc", 1, "2-4d", "core-agent", "done", "", "b8efe4a"),
    ("forms-signatures", "sign: PKCS#7 detached signature, PAdES byte-range flow (invisible/visible)", 1, "3-5d", "core-agent", "done", "", "02b8719"),
    ("forms-signatures", "UnitTestsFormSignature: form-list/form-fill/sign/verify/tamper round-trip", 1, "1-2d", "test-agent", "done",
     "Suite now 11/11.", "6a2a325"),
    ("rtl-writer", "P3: vertical mark offsets via per-glyph Ts (text rise) emission", 2, "M", "rtl-agent", "done",
     "Design: plans/archive/P3-vertical-mark-offsets.md. Emit Ts around zero-width GPOS marks so diacritics render above baseline. RED pixel-probe test in tst_rtladdtexttest.cpp. Fixed 109a4af (Ts emission, sign from mark ink position) + 9a4587d (flow phantom-space guard) + 4ed7ab2 (kasra band calibration). Blocked by P6 (74a1166).", "109a4af"),
    ("rtl-writer", "P6: emit spec-valid 65536-entry CIDToGIDMap stream (PDF 32000-1 9.7.4.3) — short array renders wrong glyphs in all strict renderers", 1, "M", "rtl-agent", "done",
     "Verified with Ghostscript 2026-08-05: short array [0,681]/[0,1173,728,1266] falls back to Identity; PDF4QT reads streams only (pdffont.cpp isStream). Blocks P3 verification. Fixed 74a1166: Flate stream, code->gid, unused=0.", "74a1166"),
    # M9–M10 shipped tasks.
    ("rtl-writer", "P1: ligature degradation in extraction — /ActualText overlay in PDFTextLayoutGenerator + writer emits full cluster text", 1, None, "rtl-agent", "done", "Wave-1 M9", "800f91e"),
    ("rtl-writer", "P2: decomposed-yeh duplication in extraction — /ActualText overlay dedups یی→ی", 1, None, "rtl-agent", "done", "Wave-1 M9", "800f91e"),
    ("rtl-search", "R#3: presentation-form NFKC pass pinned by regression test (fails if removed)", 1, None, "rtl-agent", "done", "Wave-1 M9", "271bfe8"),
    ("cli", "Page ops CLI — rotate, move-page, delete-page + UnitTestsPageOps (12/12)", 1, None, "core-agent", "done", "Wave-1 M9", "411d8f7"),
    ("tests", "W1 hosted CI — GitHub Actions gate/asan/format + vcpkg binary cache", 1, None, "infra-agent", "done", "Wave-1 M9", "88413dc"),
    ("cli", "W7 packaging — deterministic scripts/package.sh (sha256-reproducible tarball + optional deb)", 1, None, "infra-agent", "done", "Wave-1 M9", "78f608d"),
    ("tests", "W8 tracking — tracked 1000-page perf benchmark scripts/benchmark.sh (5s threshold, JSON, non-gating CI)", 1, None, "infra-agent", "done", "Wave-1 M9", "b197861"),
    ("rtl-search", "S#1 cross-line search — extend joined-visual-string search past the \\n boundary (currently cross-item only)", 1, None, "rtl-agent", "done", "M10 wave-2; depends on P1/P2 extraction fixes (landed). DB #28", "0cf3338"),
    ("object-deletion", "Object-level redaction — wire upstream redact as an albdf CLI command", 1, None, "core-agent", "done", "M10 wave-2; depends on #24 CLI registry (free). DB #29", "3a7568b"),
    ("tests", "W5 fuzzing — input fuzz harness for CLI commands", 1, None, "infra-agent", "done", "M10 wave-2; depends on #25 run-ci.sh (hosted CI). DB #30", "8d935a1"),
    ("cli", "QFlags 32-bit overflow: page-ops flags (Rotate/MovePage/DeletePage) past bit 31 broke build on Qt 6.8/6.10 — replaced with 64-bit Options class", 1, None, "yolka", "done", "Blocked all M10 branches; verified fix + ctest 12/12", "4bb644f"),
    ("cli", "F#1: render --page-first 0 / --page-last 999999999 → SIGABRT 134 (std::out_of_range, (size_t)-1) from Qt Concurrent worker — fuzz-found", 1, None, "core-agent", "done", "Reproducers /tmp/fuzz-final/fail/cli_render_page0.* (also cli_render_huge_last); found by scripts/fuzz.sh (DB #30). Separate fix dispatch.", "7a70767"),
    ("cli", "F#2: render --image-res-dpi >=10000 hangs (94 GP image, resource exhaustion) — fuzz-found", 1, None, "core-agent", "done", "Reproducer /tmp/fuzz-final/fail/cli_render_huge_dpi.*; found by scripts/fuzz.sh (DB #30). Separate fix dispatch.", "7a70767"),
    ("tests", "GH-hosted CI green: toolchain action fixed (vcpkg bootstrap, Qt archives+ICU73, fontconfig) + benchmark comma bug", 1, None, "yolka", "done", "Run 31165712432 all 5 jobs success", "d009c4b"),
    ("research", "M11 public hardening: branch protection ruleset active + first public release albdf 0.2.0", 1, None, "yolka", "done", "Ruleset 'pr check' (PR+1 approval, no deletions/force-push); release notes in docs/RELEASES.md; tag+GH release pending gate", "5b2e578"),
    ("research", "Release albdf 0.2.0 published (tag + GH release + deterministic tarball)", 1, None, "yolka", "done", "Published 2026-08-07T14:00:37Z; assets verified sha256 905d5510e3085fb57aaf0ef24bc88f7e35f1453b2b65016e2835dba83d725262", "0.2.0"),
    # M12–M14 shipped.
    ("rtl-search", "M13 GUI RTL search wiring: GUI search -> PDFTextSearchEngine (adapter + 2 widgets)", 1, None, "yolka", "done", "pdfwidgetrtlsearch adapter; PDFFindTextTool + PDFAdvancedFindWidget plain-text branch; regex stays legacy", "088c1b09"),
    ("rtl-writer", "M13 R#4: preserve /ActualText marked content on content re-edit", 1, None, "yolka", "done", "processor overrides + builder re-emit; RED 1d7b8c69, GREEN 80548d8c", "80548d8c"),
    ("rtl-search", "M13 S#3: visual-order lam-alef collapse in document-level search", 1, None, "yolka", "done", "GUI adapter exposed; 4cabbf7a", "4cabbf7a"),
    ("rtl-writer", "M13 RTL clipboard re-inversion (logical-order copy)", 2, None, "yolka", "done", "invertToLogical + onActionCopyText; a8b2a7c0 + b48bcd40", "b48bcd40"),
    ("forms-signatures", "M14 RTL form-field AP: headless form-fill emits NO appearance stream (no-op); fix to embed shaped Type0 font via PDFRTLTextEngine + add --font option", 1, None, "yolka", "done", "M14 shipped: form-field AP via PDFRTLTextEngine + --font; R#5 resolved. Merge b149864 (impl 21c4287).", "b149864"),
    ("rtl-writer", "M14 RTL FreeText annotation AP: QPainter path cannot embed Type0; additive early-return branch in updateAnnotationAppearanceStreams", 1, None, "yolka", "done", "M14 shipped: FreeText RTL AP embeds shaped Type0 font. Merge 4b9a5dd (impl 507ff44).", "4b9a5dd"),
    # v3 roadmap (plan §R0–R2), open.
    ("cli", "R1: top-level exception guard + enforce exit-code contract (unknown cmd/args)", 1, "S", "cli-agent", "open", "plan v3 R1.1/R1.2; guard is defense-in-depth (a main() guard cannot catch Qt Concurrent worker-thread exceptions; F#1 was fixed by render page-range validation 7a70767); unknown command currently exits 0", None),
    ("tests", "R1: CLI exit-code contract regression test (QProcess, real binary)", 1, "S", "test-agent", "open", "plan v3 R1.3; depends on R1.2", None),
    ("tests", "R2: shared runTool test helper + add_albdf_test() CMake function", 1, "S", "test-agent", "open", "plan v3 R2.1/R2.2; dedupe 8 runTool copies + 16 CMake blocks", None),
    ("tests", "R2: scripts/check-slop.sh anti-slop gate + wire into CI/pre-commit", 1, "M", "infra-agent", "open", "plan v3 R2.3; enforces coding-standard §11", None),
    ("fork-base", "R0: plan hygiene (archive plans, fix RELEASES claim, refresh seed.py)", 1, "S", "docs-agent", "open", "plan v3 R0.2/R0.3/R0.4", None),
]

DECISIONS = [
    # (adr_file, title, status, summary)
    ("docs/decisions/0001-fork-pdf4qt.md", "Fork PDF4QT (MIT) as the base library+CLI", "accepted",
     "Instead of greenfield, fork MIT PDF4QT: keep Pdf4QtLibCore + PdfTool, strip GUI. 4-8x cheaper than greenfield."),
    ("docs/decisions/0002-no-gui-in-v1.md", "No GUI in v1 — library + CLI only", "accepted",
     "User directive. Everything must be headless-testable; future GUI consumes the library."),
    ("docs/decisions/0003-rtl-differentiator.md", "RTL write+search is the differentiator and is greenfield", "accepted",
     "PDF4QT has zero bidi/HarfBuzz; build with FriBidi + HarfBuzz + ToUnicode + ActualText."),
    ("docs/decisions/0004-license-posture.md", "License posture: MIT fork + permissive deps", "superseded",
     "Fork keeps MIT; add HarfBuzz (MIT) + FriBidi (LGPL-2.1, dynamic link OK). No AGPL/GPL deps in core. Superseded by ADR-0005."),
    ("docs/decisions/0005-gpl-license.md", "Relicense to GPL-3.0-or-later", "accepted",
     "Authored code GPL-3.0-or-later; upstream PDF4QT portions keep MIT. Deps stay GPLv3-compatible."),
    ("docs/decisions/0006-forms-signatures-cli.md", "Forms and digital signatures via CLI", "accepted",
     "Expose upstream AcroForm engine + PKCS#7 signing through form-list/form-fill/sign/verify-signatures."),
]

QUESTIONS = [
    # (question, asked_to, status, answer)
    ("Project/repo final name? Working name: 'pdfedit'", "user", "answered", "albdf — repo yolka-wiz/al-bdf-engine"),
    ("Git hosting: GitHub, KDE Invent, or local-only for now?", "user", "answered", "GitHub (public): yolka-wiz/al-bdf-engine"),
    ("Do we add the upstream PDF4QT as a git remote for cherry-picking, or fully sever?", "user", "answered", "Kept — upstream remote for cherry-picks"),
    ("context7 MCP: user said they'll provide API key — what other keys/credentials are needed?", "user", "answered", "Key received + wired via Hermes MCP; keep it out of the repo"),
    ("Confirm: keep existing PdfTool ~30 commands as-is and ADD new ones (delete-object, add-text)?", "user", "answered", "Confirmed — keep all upstream commands, add new ones"),
    ("Which agent roles to define? Proposed: core-agent, cli-agent, rtl-agent, test-agent, research(rosetta), orchestrator(yolka)", "user", "answered", "Confirmed — all six roles"),
]

def main():
    if not os.path.exists(DB):
        print("DB missing — run `python3 scripts/db.py init` first", file=sys.stderr)
        sys.exit(1)
    c = sqlite3.connect(DB)
    for (cid, name, status, owner, desc, sdir) in COMPONENTS:
        c.execute("INSERT OR IGNORE INTO components(id,name,status,owner_agent,description,source_dir) VALUES (?,?,?,?,?,?)",
                  (cid, name, status, owner, desc, sdir))
    for (cid, title, prio, est, assignee, status, notes, ref) in TASKS:
        c.execute("INSERT INTO tasks(component_id,title,priority,effort_estimate,assignee,status,notes,evidence_ref) VALUES (?,?,?,?,?,?,?,?)",
                  (cid, title, prio, est, assignee, status, notes, ref))
    for (file, title, status, summary) in DECISIONS:
        c.execute("INSERT INTO decisions(adr_file,title,status,summary) VALUES (?,?,?,?)",
                  (file, title, status, summary))
    for (q, asked, status, answer) in QUESTIONS:
        c.execute("INSERT INTO questions(question,asked_to,status,answer) VALUES (?,?,?,?)",
                  (q, asked, status, answer))
    c.commit()
    print(f"Seeded: {len(COMPONENTS)} components, {len(TASKS)} tasks, {len(DECISIONS)} decisions, {len(QUESTIONS)} questions")

if __name__ == "__main__":
    main()
