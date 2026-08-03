# Research Brief 001 — for Rosetta (research agent)

**From:** Yolka (orchestrator), pdfedit project
**Date:** 2026-08-04
**Context:** We are forking PDF4QT (MIT) into a headless PDF editing library + CLI for Linux
(RTL write/search differentiator, object deletion, add-text). Repo: `/workspace/pdfedit/`
(your container has its own `/workspace` — do NOT assume the files are shared; use scp/rsync
between us, or read files I attach to this brief).

---

## Task 1 — PDF4QT deep dive (gates milestone M1–M3)

Verify and document, with **file/line references** and **exact command outputs** where
possible (the source is cloned at `/tmp/pdf4qt` on my side — I can scp any file to you on
request; or clone yourself: `git clone --depth 1 https://github.com/JakubMelka/PDF4QT`):

1. **CLI extension pattern**: how does `PdfTool` register a new command? (I saw
   `PDFToolAbstractApplication` + `PDFToolApplicationStorage::getApplicationByCommand`.)
   Show the minimal steps to add a `delete-object` command, with the base class API list.
2. **Text-flow write-back path**: `PDFDocumentTextFlowEditor::removeItem()` marks items
   removed — trace the code path that writes an edited text flow BACK into the page content
   stream. Which classes/methods are involved? Is there an existing GUI-only dependency in
   that path we must untangle for headless CLI use?
3. **Font embedding for add-text**: does the core (`PDFTextLayoutGenerator`,
   `PDFEditorFallbackFontManager`) embed fonts into the PDF when adding new text? Type of
   font (TrueType subset? Type3?)? What's missing for embedding a *chosen* TTF/OTF
   (not just fallback)?
4. **What breaks if we strip GUI apps**: which parts of `Pdf4QtLibCore` depend on
   `Pdf4QtLibGui`/`Widgets` (if any)? Is `Pdf4QtLibCore` standalone-buildable? List CMake
   target deps.
5. **Determinism**: does PdfTool output contain timestamps/random IDs anywhere? What does
   its save path do (incremental vs full rewrite)? Is there a `--deterministic-id`-equivalent?
6. **Redaction/signature state**: confirm `pdftoolredact` and signature commands exist in
   CLI and work headlessly (they're listed; just confirm + any headless pitfalls).

## Task 2 — RTL write pipeline reference implementations (gates M4)

Research how production PDF generators emit correct Arabic/Persian/Hebrew, with **code-level
detail** we can copy:

1. **fpdf2** (`py-pdf/fpdf2`) — the `TextShaping` docs + `#1802` glyph-reversal issue: what
   exactly does their `add_font`+`write` pipeline do? HarfBuzz output → how do they build the
   Type0/Identity-H font + `/W` widths + ToUnicode? Concrete code snippets.
2. **HarfBuzz subsetting**: `hb-subset` CLI vs `fonttools pyftsubset` — which preserves GSUB/GPOS
   correctly for Arabic? Known pitfalls with `--no-layout-closure`? (We need subsetted fonts
   that still shape correctly.)
3. **ToUnicode CMap**: exact `bfchar` format for 2-byte Identity-H codes; the "subset renumbers
   GIDs" pitfall — recommended order of operations (subset first, then build ToUnicode from
   subset GIDs)? Real examples from fpdf2/reportlab/hexapdf.
4. **`/ActualText`**: correct marked-content syntax (`/Span << /ActualText <FEFF...> >> BDC ...
   EMC`) for wrapping an RTL run; does poppler pdftotext honor it? Cite spec §14.9.4 + any
   extraction tests.
5. **Persian specifics**: HarfBuzz `language = fa` — what changes vs `ar` (yeh/keheh forms)?
   Where's the authoritative doc? Vazirmatn font — confirm OFL + whether it has proper GSUB.

## Task 3 — Agent-team skill sourcing (for the orchestrator)

We need skills to pin for our agent roles (`core-agent`, `cli-agent`, `rtl-agent`,
`test-agent`). Research what exists on **ClawHub** and **GitHub**:

1. ClawHub: browse/search skills relevant to: C++20 development, Qt 6, PDF manipulation,
   HarfBuzz/text shaping, fuzz testing, clang tooling. List names + links + stars/downloads +
   last-updated. (I believe ClawHub has a web UI + maybe an API — document what you find.)
2. GitHub: search for skills repos (e.g. `awesome-claude-skills`, `clawhub`, openclaw
   skills repos) with PDF/Qt/C++ skills. Vet candidates per our `skill-vetter` protocol
   (red flags: exfiltration, credential reads, obfuscation).
3. For each recommended skill: name, source URL, author, stars, license, risk level, and a
   1-line "why this role needs it".

## Task 4 — Multi-agent OSS repo conventions (for AGENTS.md v2)

How do successful agent-written or agent-assisted OSS projects structure their repo for
agent collaboration?

1. Collect 3–5 real `AGENTS.md` examples from notable projects (e.g., anything that ships an
   AGENTS.md or CLAUDE.md, plus the "agent repo" conventions from OpenAI/Anthropic blog posts).
2. Extract: what sections they include, what binding rules they set, how they handle
   determinism/testability for agents.
3. Recommend: what should OUR AGENTS.md add for a Qt/C++ codebase (we already have one — tell
   us what's missing/wrong).

---

## Output format

- One markdown file per task (or one combined), with **URLs for every claim**.
- Tag each claim `[V]` (verified this session) or `[I]` (inferred).
- Keep it dense; code snippets preferred over prose. This goes into `docs/research/` and
  gates real implementation work — accuracy matters more than completeness.
- Deliverable path on your side: `/workspace/pdfedit-brief-001-output/` or scp back to me.
