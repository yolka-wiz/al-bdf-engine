#!/usr/bin/env python3
"""Seed the albdf tracking DB with the current known state (2026-08-05).

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
    ("tests", "Golden-image harness (deterministic render diff)", 1, "1-2w", "test-agent", "done", "", "8b9d5bd"),
    ("tests", "RTL corpus: Persian/Arabic/Hebrew fixtures incl. ZWNJ, lam-alef, tashkeel, digits", 2, "1-2w", "test-agent", "done", "", "aa92bee"),
    ("tests", "CI: offscreen ctest + ASAN job", 2, "1w", "test-agent", "done", "", "dc45fcb"),
    ("forms-signatures", "form-list: enumerate AcroForm fields (name, type, value, page, rect)", 1, "1-3d", "core-agent", "done", "", "3b3e563"),
    ("forms-signatures", "form-fill: set field values + regenerate appearances, write new doc", 1, "2-4d", "core-agent", "done", "", "b8efe4a"),
    ("forms-signatures", "sign: PKCS#7 detached signature, PAdES byte-range flow (invisible/visible)", 1, "3-5d", "core-agent", "done", "", "02b8719"),
    ("forms-signatures", "UnitTestsFormSignature: form-list/form-fill/sign/verify/tamper round-trip", 1, "1-2d", "test-agent", "done",
     "Suite now 11/11.", "6a2a325"),
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
