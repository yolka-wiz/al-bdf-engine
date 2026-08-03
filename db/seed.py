#!/usr/bin/env python3
"""Seed the pdfedit tracking DB with the initial known state (2026-08-04).

Run after `python3 scripts/db.py init`. Idempotent-ish: components are
INSERT OR IGNORE, tasks/decisions/questions are inserted fresh (run once).
"""
import os, sqlite3, sys

DB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "db", "pdfedit.db")

COMPONENTS = [
    # (id, name, status, owner_agent, description, source_dir)
    ("fork-base", "PDF4QT fork: Pdf4QtLibCore + PdfTool", "planned", "core-agent",
     "Vendored fork of MIT PDF4QT; upstream remote tracked; GUI apps stripped. Everything else builds on this.",
     "src/"),
    ("cli", "CLI surface (PdfTool extension)", "planned", "cli-agent",
     "Extend PdfTool with delete-object / add-text commands; deterministic output; existing ~30 commands kept.",
     "src/cli/"),
    ("text-recognition", "Text recognition as objects", "planned", "core-agent",
     "PDFDocumentTextFlow::Item with bbox + char boxes already in PDF4QT core; verify + expose via CLI (fetch-text).",
     "src/core/"),
    ("object-deletion", "Whole-object deletion (text runs, images, elements)", "planned", "core-agent",
     "Wire PDFDocumentTextFlowEditor::removeItem + PDFPageContentEditorProcessor to CLI delete-object. Image refcount/nesting edge cases.",
     "src/core/"),
    ("add-text", "Add text (LTR first) via CLI", "planned", "core-agent",
     "PDFTextLayoutGenerator exists; need CLI add-text with font embedding + positioning + write-back.",
     "src/core/"),
    ("rtl-writer", "RTL write pipeline (Arabic/Persian/Hebrew)", "planned", "rtl-agent",
     "GREENFIELD: FriBidi bidi + HarfBuzz shaping + Type0/Identity-H embed + ToUnicode CMap + ActualText. The differentiator.",
     "src/core/"),
    ("rtl-search", "RTL-aware search", "planned", "rtl-agent",
     "Normalize + bidi inversion on top of existing text flow extraction. No OSS incumbent does this.",
     "src/core/"),
    ("tests", "Test infrastructure: unit + golden + CLI", "planned", "test-agent",
     "Deterministic golden-image tests, RTL corpus (ZWNJ/lam-alef/tashkeel/digits), offscreen CI.",
     "src/tests/"),
    ("research", "Research stream (Rosetta)", "in_progress", "rosetta",
     "Deep dives: PDF4QT API surface, RTL reference impls, skill sourcing, AGENTS.md best practice.",
     "docs/research/"),
]

TASKS = [
    # (component_id, title, priority, estimate, assignee, status, notes)
    ("fork-base", "Vendor PDF4QT fork into src/, strip GUI apps (Viewer/Editor/PageMaster/Diff/LaunchPad)", 1, "2-4d", "core-agent", "open",
     "Keep Pdf4QtLibCore + PdfTool + UnitTests; add upstream remote for cherry-picking."),
    ("fork-base", "Verify PdfTool headless run: QT_QPA_PLATFORM=offscreen build + fetch-text smoke test", 1, "1d", "core-agent", "open",
     "PdfTool main() uses QGuiApplication; confirm offscreen works on bare container."),
    ("cli", "Add delete-object CLI command (wire TextFlowEditor::removeItem + write-back)", 1, "2-4d", "core-agent", "open",
     "Reads page + object id/rect, removes, writes incremental save."),
    ("cli", "Add add-text CLI command (LTR)", 2, "3-6d", "core-agent", "open",
     "Text + position + font; reuse PDFTextLayoutGenerator; needs font embedding path."),
    ("object-deletion", "Image XObject reference-counting + Form XObject nesting for deletion", 2, "3-5d", "core-agent", "open",
     "Don't orphan shared resources; recurse into nested forms; inline images BI..EI."),
    ("rtl-writer", "Add HarfBuzz + FriBidi deps to build (MIT/LGPL)", 1, "1-2d", "rtl-agent", "open", ""),
    ("rtl-writer", "RTL add-text: bidi runs + HarfBuzz shaping -> visual-order Tj", 1, "4-6w", "rtl-agent", "open",
     "Absolute-position Tm per run; PDF has no kerning, bake advances."),
    ("rtl-writer", "ToUnicode CMap from HarfBuzz clusters + ActualText marked content", 1, "1-2w", "rtl-agent", "open",
     "Subset GIDs pitfall; map lam-alef to 2 codepoints."),
    ("rtl-search", "Normalization: tashkeel, presentation forms, lam-alef, Persian/Arabic unification, digits, ZWNJ", 2, "2-3w", "rtl-agent", "open", ""),
    ("rtl-search", "Bidi inversion of extracted visual-order text for search matching", 2, "2-3w", "rtl-agent", "open",
     "Pure-RTL = straight reversal; mixed lines need level-based run split."),
    ("tests", "Golden-image harness (deterministic render diff)", 1, "1-2w", "test-agent", "open", ""),
    ("tests", "RTL corpus: Persian/Arabic/Hebrew fixtures incl. ZWNJ, lam-alef, tashkeel, digits", 2, "1-2w", "test-agent", "open", ""),
    ("tests", "CI: offscreen ctest + ASAN job", 2, "1w", "test-agent", "open", ""),
]

DECISIONS = [
    # (adr_file, title, status, summary)
    ("docs/decisions/0001-fork-pdf4qt.md", "Fork PDF4QT (MIT) as the base library+CLI", "proposed",
     "Instead of greenfield, fork MIT PDF4QT: keep Pdf4QtLibCore + PdfTool, strip GUI. 4-8x cheaper than greenfield."),
    ("docs/decisions/0002-no-gui-in-v1.md", "No GUI in v1 — library + CLI only", "proposed",
     "User directive. Everything must be headless-testable; future GUI consumes the library."),
    ("docs/decisions/0003-rtl-differentiator.md", "RTL write+search is the differentiator and is greenfield", "proposed",
     "PDF4QT has zero bidi/HarfBuzz; build with FriBidi + HarfBuzz + ToUnicode + ActualText."),
    ("docs/decisions/0004-license-posture.md", "License posture: MIT fork + permissive deps", "proposed",
     "Fork keeps MIT; add HarfBuzz (MIT) + FriBidi (LGPL-2.1, dynamic link OK). No AGPL/GPL deps in core."),
]

QUESTIONS = [
    # (question, asked_to, status, answer)
    ("Project/repo final name? Working name: 'pdfedit'", "user", "open", None),
    ("Git hosting: GitHub, KDE Invent, or local-only for now?", "user", "open", None),
    ("Do we add the upstream PDF4QT as a git remote for cherry-picking, or fully sever?", "user", "open", None),
    ("context7 MCP: user said they'll provide API key — what other keys/credentials are needed?", "user", "open", None),
    ("Confirm: keep existing PdfTool ~30 commands as-is and ADD new ones (delete-object, add-text)?", "user", "open", None),
    ("Which agent roles to define? Proposed: core-agent, cli-agent, rtl-agent, test-agent, research(rosetta), orchestrator(yolka)", "user", "open", None),
]

def main():
    if not os.path.exists(DB):
        print("DB missing — run `python3 scripts/db.py init` first", file=sys.stderr)
        sys.exit(1)
    c = sqlite3.connect(DB)
    for (cid, name, status, owner, desc, sdir) in COMPONENTS:
        c.execute("INSERT OR IGNORE INTO components(id,name,status,owner_agent,description,source_dir) VALUES (?,?,?,?,?,?)",
                  (cid, name, status, owner, desc, sdir))
    for (cid, title, prio, est, assignee, status, notes) in TASKS:
        c.execute("INSERT INTO tasks(component_id,title,priority,effort_estimate,assignee,status,notes) VALUES (?,?,?,?,?,?,?)",
                  (cid, title, prio, est, assignee, status, notes))
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
