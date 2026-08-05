# FreeType docs (Context7, fetched 2026-08-04)

Source: https://context7.com/freetype/freetype — resolved via MCP `resolve-library-id`
(`/freetype/freetype`, High reputation, 149 snippets). Library ID for future
`query-docs` calls: `/freetype/freetype`.

## Glyph loading pipeline

```c
FT_Load_Glyph(face, glyph_index, load_flags);
// Without FT_LOAD_RENDER: only the outline is loaded (cheap; no rasterization).
// With FT_LOAD_RENDER: FT_Load_Glyph internally calls FT_Render_Glyph.
```

Key load flags (PDF4QT uses `FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING` for outlines):
- `FT_LOAD_NO_SCALE` — do not scale outline; metrics in font units
- `FT_LOAD_NO_HINTING`
- `FT_LOAD_RENDER` — rasterize
- Default (no NO_SCALE): glyph slot metrics in 26.6 fixed point, scaled to current size

## 26.6 fixed point

- 26 bits integer + 6 bits fraction; distance between adjacent pixels = 64 units
- `slot->advance.x` / `advance.y` are in 26.6 format (64 units per pixel at size)
- `slot->metrics.horiAdvance` also 26.6
- Outline coordinates passed to `FT_Outline_Funcs` callbacks are in 26.6 units too
  (with FONT_MULTIPLIER scaling in PDF4QT: `FONT_MULTIPLIER = 64 / PIXEL_SIZE_MULTIPLIER`)

## Rasterizer / outlines

- Glyph = multiple closed contours; flags per point: 'on' or 'off' curve
- Two consecutive 'on' points = line segment
- 'off' point between two 'on' points = quadratic (conic) Bézier, control point = 'off'
- Four points with two 'off' controls between 'on' endpoints = cubic Bézier
- `FT_Outline_Decompose(face->glyph->outline, &funcs, &user)` walks move_to/line_to/
  conic_to/cubic_to

## Key notes for albdf M4

- PDF4QT `PDFRealizedFontImpl::getGlyph` calls
  `FT_Load_Glyph(m_face, glyphIndex, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)`
  then `FT_Outline_Decompose`; `glyph.advance = slot->advance.x * FONT_MULTIPLIER`
- FT error 6 = `FT_Err_Invalid_File_Format` — face data invalid (seen when the
  embedded FontFile2 stream was malformed or the face was NULL)
- When embedding fonts: FontFile2 must be a valid TTF; Length must match the
  compressed stream exactly or the reader truncates and FT fails on load
