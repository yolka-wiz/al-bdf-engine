# GNU FriBidi docs (Context7, fetched 2026-08-04)

Source: https://context7.com/fribidi/fribidi — resolved via MCP `resolve-library-id`
(`/fribidi/fribidi`, High reputation, 119 code snippets). Library ID for future
`query-docs` calls: `/fribidi/fribidi`.

## fribidi_log2vis — one-call logical→visual

```c
#include <fribidi.h>

FriBidiChar logical[] = {'c', 'a', 'r', ' ', 'i', 's', ' ', 'T', 'H', 'E', ' ', 'C', 'A', 'R'};
FriBidiStrIndex len = 14;

FriBidiChar visual[64];
FriBidiStrIndex ltov[64];  // logical→visual position map (may be NULL)
FriBidiStrIndex vtol[64];  // visual→logical position map (may be NULL)
FriBidiLevel levels[64];   // embedding level per character (may be NULL)

FriBidiParType base_dir = FRIBIDI_PAR_ON;   // auto-detect from content

FriBidiLevel max_level = fribidi_log2vis(
    logical, len, &base_dir, visual, ltov, vtol, levels
);
// returns max embedding level; 0 = error
// Even levels = LTR, odd levels = RTL
```

## Lower-level API (what log2vis wraps)

1. `fribidi_get_bidi_types(str, len, bidi_types)` — character types (L, R, AL, AN, ...)
2. `fribidi_get_bracket_types(str, len, bidi_types, bracket_types)` — bracket pairing (Unicode 6.3+)
3. `fribidi_get_par_embedding_levels_ex(bidi_types, bracket_types, len, &base_dir, levels)`
   — full UAX#9 with bracket resolution
4. `fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT, bidi_types, len, offset, base_dir, levels, str, position_map)`
   — reorder in place to visual order

## Semantics

- **Levels**: even = LTR, odd = RTL. Level 0 = paragraph base direction (LTR);
  RTL paragraphs get base level 1... (levels are odd for RTL runs)
- `base_dir` values: `FRIBIDI_PAR_ON` (auto), `FRIBIDI_PAR_LTR`, `FRIBIDI_PAR_RTL`
- Reorder flags: `FRIBIDI_FLAGS_DEFAULT` enables mirroring + NSM reordering
- Max return level 0 = failure

## Key notes for pdfedit M4

- To get directional runs: call `fribidi_get_par_embedding_levels_ex` (or log2vis to
  get levels), then split the logical string into maximal runs of equal
  level-parity (level & 1 → RTL or LTR run)
- Do NOT feed FriBidi's visual string to HarfBuzz — shape each logical run with
  HB direction = RTL/LTR per level parity; HB itself handles the per-run
  right-to-left glyph output
- Persian/Arabic/Hebrew: base_dir = FRIBIDI_PAR_RTL for RTL-first paragraphs
