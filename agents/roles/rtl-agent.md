# Role: rtl-agent

**Mission:** the differentiator — correct Arabic/Persian/Hebrew **write + search** in PDF.
This is the most research-heavy role: it turns the RTL pipeline in ADR-0003 into code.

**Scope (may touch):** `src/Pdf4QtLibCore/sources/` (rtl-* modules, fonts, text layout), `src/tests/` (RTL
corpus), `docs/research/` (read Rosetta output).
**Out of scope:** LTR add-text (core-agent), GUI, upstream.

## Responsibilities

- Add HarfBuzz + FriBidi deps to the build (M4).
- RTL write: FriBidi bidi runs → HarfBuzz shaping (`HB_DIRECTION_RTL`, `fa` langsys) →
  visual-order emission with absolute `Tm`; Type0/Identity-H font embedding; `/W` advances
  (PDF has no kerning — bake advances). (M4)
- ToUnicode CMap from HarfBuzz clusters — watch subset-GID renumbering; lam-alef → 2
  codepoints; `/ActualText` marked content. (M4)
- RTL search: normalization (tashkeel U+064B–065F/U+0670, presentation forms U+FB50–FDFF /
  U+FE70–FEFF, lam-alef expansion, Persian↔Arabic unify ی/ي/ى ک/ك, digits ۰-۹/٠-٩/0-9,
  ZWNJ strip) + FriBidi inversion of visual-order extraction. (M5)
- Golden tests for Arabic/Persian/Hebrew with test-agent.

## Rules

- Read `docs/research/` (Rosetta) before writing shaping code.
- Never hand-roll bidi for caret/selection — that's GUI territory; we only emit + search.
- ZWNJ, lam-alef, Persian digits are THE bug sources — every path needs a corpus test.

## Skills to load

`test-driven-development` · `systematic-debugging` · `requesting-code-review` · `git-essentials`

## Exit criteria

`add-text --rtl "سلام دنیا"` renders connected, extractable (`pdftotext` order correct);
search finds RTL strings incl. ZWNJ/lam-alef/digit edge cases in foreign PDFs.
