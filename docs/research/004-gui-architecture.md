# GUI Architecture — how a desktop frontend consumes albdf

> Status: **THINKING / design note** (2026-08-05, yolka). Not a decision, not a
> plan to execute — ADR-0002 still defers the GUI. This records how the binary
> would be consumed so the engine keeps the right seams, and so the GUI work,
> when it starts, doesn't have to redesign the CLI.
> Self-critique: severity table at the bottom (cloud-eng lens, per user pref).

---

## 1. The central question: library mode or binary mode?

The repo's ADR-0002 says "a thin Qt shell or PDF4QT's existing apps consumes the
library." But the user framing is **"how the GUI utilizes this binary"** — so
both integration shapes matter. They are not mutually exclusive.

| Mode | Shape | Latency | RTL complexity in GUI | Coupling |
|---|---|---|---|---|
| **A. Library mode** | Qt GUI links `Pdf4QtLibCore` (engine survived the M1 strip; widgets did not — re-import `PDFViewerWidget` from upstream remote or build own canvas) | Best (in-process, no IPC) | High — GUI sees bidi/shaping internals | Tight ABI/C++ |
| **B. Binary mode** | GUI spawns `albdf` per op (or long-lived session later); JSON in/out | 30–80 ms/op — fine for v1 | **Zero** — GUI sends text+coords, binary does shaping | Loose, language-agnostic (Qt, QML, Python, Electron all viable) |

**Recommended direction: B (binary/CLI as the engine), with the CLI contract
treated as the API.** Rationale:

1. **All RTL complexity stays in the tested headless core.** Bidi, HarfBuzz
   shaping, GPOS mark offsets, ToUnicode, ActualText — none of that leaks into
   the GUI. The GUI is RTL-agnostic: it places text and displays the rendered
   result. This is the single biggest win of process mode for this product.
2. **Latency is already proven fine**: perf smoke shows 0.03s/page render,
   0.04s search, 0.02s delete at 72 dpi. Human interaction (typing, clicks)
   tolerates 30–80 ms per operation; live preview can be debounced.
3. **Determinism is the testability lever**: every GUI behavior can be
   reproduced headlessly against the same JSON the GUI sees. The GUI becomes a
   thin presentation layer with the entire backend contract testable by agents.
4. **ADR-0002 stays intact**: no GUI in the fork repo. The GUI can live in a
   separate repo consuming the stable CLI, keeping the core repo lean.

Library mode remains the escape hatch if a future feature needs sub-second
interactive shaping (e.g. live drag-to-place text) — the engine seam (lib +
CLI both built from the same sources) makes that a later, additive choice.

---

## 2. What the GUI needs from the binary (today vs. gaps)

| GUI need | Today in `albdf` | Gap |
|---|---|---|
| Open document / metadata | `info` (text table) | JSON form |
| Render page to pixels | `render` → PNG @ dpi | Tile/crop render for huge pages; dpi param exists |
| Read text + geometry | `fetch-text` (text), `recognize-text` (items) | **Item bboxes in JSON** for hit-testing & overlays |
| Search with spans | `search-text` (Item column first+last) | **Match rects in JSON** for highlight overlays |
| Add text LTR/RTL | `add-text --rtl --font --lang` | None (this is the flagship) |
| Delete objects | `delete-object` (rect/object) | None |
| Forms | `form-list`, `form-fill` | JSON field tree |
| Signatures | `sign` (PKCS#7, PAdES) | Cert/key selection UX |
| Undo/redo | none in CLI (functional ops) | GUI keeps **snapshot stack** or **op log** (deterministic ops make this trivial) |
| Long-running session | none (spawn per op) | Optional later `--daemon`/stdin-loop mode |

**The single most important gap: a machine-readable output mode.** Today the
formatter prints human tables. GUI work should add `--json` (or `--format json`)
to every command: info, render metadata, fetch-text/recognize-text (with item
bboxes), search-text (with match rects), form-list, add/delete/sign results
(exit code already exists — keep it, add JSON body). This is engine work,
testable headlessly, and unlocks every GUI shape.

---

## 3. GUI layer sketch (thin, KDE-friendly)

- **Canvas**: QML or QWidgets page view; renders PNG tiles via `albdf render`
  (spawn + cache), zoom = re-render at higher dpi (already supported). Overlay
  layer draws search-match rects and selection boxes from JSON geometry.
- **Document ops**: all edits call `albdf <op> --json` on a working copy;
  on success swap the working copy and refresh affected pages.
- **Undo/redo**: keep a snapshot stack of the working PDF (files are small, ops
  are deterministic) — or an op log replayed from the last snapshot. No engine
  support needed.
- **RTL editing flow**: text field (LTR/RTL toggle) → debounced preview =
  `add-text` to a throwaway page + `render` → commit on confirm. The GUI never
  shapes; it shows pixels.
- **KDE specifics**: Wayland-safe (no X11 calls; offscreen render only),
  KIO integration for open/save, standard shortcuts, `io.github`-style
  app-id if we ever package it.

---

## 4. Engine seams to preserve (so GUI work stays cheap)

1. Every capability reachable via CLI with `--json` — **the CLI is the API**.
2. Deterministic output (already a repo law) — GUI snapshots rely on it.
3. `render` dpi/crop flexibility — needed for zoom and tile rendering.
4. Geometry export (item bboxes, match rects) — needed for hit-testing.
5. Keep spawn-per-op fast (30–80 ms) — defer daemon mode until proven needed.

---

## 5. Suggested GUI milestones (when the time comes)

| M | Deliverable | Depends on |
|---|---|---|
| G1 | `--json` across commands + geometry export (engine, headless-testable) | none |
| G2 | Minimal viewer: open → render tiles → zoom/pan → fetch-text overlay | G1 |
| G3 | Edit ops: add-text (incl. RTL), delete-object, search-highlight, snapshot undo | G1 |
| G4 | Forms/sign UX + KDE packaging | G1–G3 |

---

## 6. Self-critique (cloud-eng lens)

| Severity | Issue | Note |
|---|---|---|
| **HIGH** | Spawn-per-op means every edit rewrites the working file; if ops ever exceed ~100 ms this UX degrades (typing preview lag) | Mitigation: debounce preview; later daemon mode. Don't build daemon now — YAGNI until a real GUI measures it. |
| **MED** | JSON protocol is a *new public API* with versioning burden; CLI table output stays for humans → two outputs to keep consistent | Pin `--json` schema in docs; version it; both formatters feed from the same data structures. |
| **MED** | Snapshot-stack undo is O(files) — fine for v1 docs, not for 1000-page books | Perf smoke: 1000-page open/render is fast; writes dominate. Cap snapshot count; revisit if needed. |
| **LOW** | Library mode abandoned → future sub-second interactions need the re-import work (widgets were stripped) | Upstream remote still has the widget layer; import is a known path. |
| **LOW** | GUI repo divergence risk (two repos, one contract) | CI can run GUI's contract tests against the CLI in both repos. |
