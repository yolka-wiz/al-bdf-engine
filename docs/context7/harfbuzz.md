# HarfBuzz docs (Context7, fetched 2026-08-04)

Source: https://context7.com/harfbuzz/harfbuzz — resolved via MCP `resolve-library-id`
(`/harfbuzz/harfbuzz`, High reputation, 2024 code snippets). Library ID for future
`query-docs` calls: `/harfbuzz/harfbuzz`.

## Basic shaping pipeline

```c
#include "hb.h"
#include "hb-ft.h"
#include <ft2build.h>
#include FT_FREETYPE_H

FT_Init_FreeType(&ft_lib);
FT_New_Face(ft_lib, "/path/to/font.ttf", 0, &face);
FT_Select_Charmap(face, FT_ENCODING_UNICODE);

hb_font_t *hb_font = hb_ft_font_create(face, NULL);
hb_buffer_t *buf = hb_buffer_create();

hb_buffer_add_utf8(buf, text, -1, 0, -1);   // or add_utf16 / add_utf32
hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
hb_buffer_set_language(buf, hb_language_from_string("en", -1));

hb_shape(hb_font, buf, NULL);

hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, NULL);
hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, NULL);
// info[i].codepoint = glyph ID, info[i].cluster = input char index
// pos[i].x_advance / y_advance / x_offset / y_offset (font units at scale)

hb_buffer_destroy(buf);
hb_font_destroy(hb_font);
FT_Done_Face(face);
FT_Done_FreeType(ft_lib);
```

## Buffer API (management)

- `hb_buffer_create` / `hb_buffer_destroy`
- `hb_buffer_add_codepoints` / `add_utf32` / `add_utf16` / `add_utf8` / `add_latin1`
- `hb_buffer_set_direction` / `get_direction` — HB_DIRECTION_LTR(4), RTL(5), TTB(6), BTT(7)
- `hb_buffer_set_script` / `get_script`
- `hb_buffer_set_language` / `get_language`
- `hb_buffer_set_flags` / `get_flags`
- `hb_buffer_set_cluster_level` / `get_cluster_level`
  - Cluster levels: `HB_BUFFER_CLUSTER_LEVEL_IS_CHARACTERS` (0, MONOTONE_CHARACTERS),
    `HB_BUFFER_CLUSTER_LEVEL_IS_GRAPHEMES` (1, MONOTONE_GRAPHEMES), `HB_BUFFER_CLUSTER_LEVEL_IS_MONOTONE` (2, CHARACTERS)
  - fpdf2 uses level 1 (MONOTONE_GRAPHEMES)
- `hb_buffer_set_segment_properties` / `guess_segment_properties`
- `hb_buffer_set_unicode_funcs`
- `hb_buffer_get_glyph_infos` / `get_glyph_positions` / `has_positions`
- `hb_buffer_reverse` / `reverse_range` / `reverse_clusters`
  - `reverse_clusters`: reverses order but keeps items of the same cluster together
- `hb_buffer_serialize` / `serialize_glyphs` (debug output)
- `hb_buffer_diff`
- `hb_buffer_set_message_func` (diagnostics)

## Font/face/blob API

- `hb_blob_create(ptr, len, mode, user_data, destroy)` — from memory buffer
- `hb_blob_create_from_file`
- `hb_face_create(blob, index)` / `hb_face_create_or_fail`
- `hb_font_create(face)` — font = face at a scale + variation position
- `hb_font_get_glyph_h_advance(font, glyph_id)` — default horizontal advance at current scale
- `hb_font_get_glyph_v_advance(font, glyph_id)`
- `hb_font_set_scale(font, x_scale, y_scale)` — applied to metrics and coordinates
- `hb_font_get_scale`
- `hb_font_set_variations` / `hb_font_set_var_coords_*`

## Positioning semantics (glyph_position_t)

- `x_advance`, `y_advance`, `x_offset`, `y_offset` — all in font units at the current scale
- The shaper maps chars → glyphs + positions; it does NOT do outline manipulation,
  line breaks, or cross-run layout

## Key notes for PDF RTL embedding (pdfedit M4)

- Glyph IDs come from `info[i].codepoint` — these are the GIDs to emit in the PDF
- Cluster → ToUnicode: cluster values map glyphs back to input character indices
- RTL: HarfBuzz emits glyphs in visual order (rightmost first); PDF text operators
  need leftmost-first → reverse the glyph array (and clusters) for RTL runs
- Advances for the /W array: use hmtx (hb_font_get_glyph_h_advance on the nominal
  font) NOT the shaped x_advance (which includes GPOS kerning/marks)
