# ADR-0003: RTL write + search is the differentiator and is greenfield

**Status:** proposed (2026-08-04)

## Context

No OSS native PDF app does correct RTL (Arabic/Persian/Hebrew) write + search: Okular bug
353300 open since ~2015, pdf.js search has no bidi pass, Chrome renders RTL fields reversed
(research report §5, verified). PDF4QT has zero bidi/HarfBuzz code (verified in source).
The incumbent gap is the product's reason to exist.

## Decision

Build the RTL pipeline ourselves, per research report §5:

1. **Write:** logical Unicode → FriBidi bidi (UAX #9) → per-run HarfBuzz shaping
   (`HB_DIRECTION_RTL`, `fa` langsys for Persian) → emit glyphs in visual order via
   absolute-position `Tm` per run → Type0/Identity-H font embedding with `/W` advances →
   ToUnicode CMap from HarfBuzz clusters (subset-GID pitfall!) → `/ActualText` marked content.
2. **Search:** extract visual-order runs → invert with FriBidi → normalize (tashkeel,
   presentation forms, lam-alef expansion, Persian↔Arabic unify, digit unify, ZWNJ strip) →
   substring match with per-char index maps.

## Consequences

- This is 8–14 person-weeks of greenfield work — the largest single cost item.
- It is also the moat: no incumbent has it.
- RTL deletion deliberately OUT of scope (user directive: "rtl support for adding text is
  important, not deleting").
