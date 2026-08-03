# 002 — RTL reference implementations (for PDF4QT fork)

- **Date:** 2026-08-04
- **Author:** research subagent (Rosetta brief 001, task 1)
- **Status:** source-verified against fpdf2 master ([V]); behavioral claims that could not be
  executed are tagged **[I]** with a follow-up checklist at the end.
- **Claims key:** [V] = verified against fetched source/issue/docs URL or prior report;
  [I] = inferred/not executed here.
- **Blocker note:** Hermes lifecycle-guard bug (embedded-null-byte crash on `python <file>` /
  non-ASCII scripts) blocked running the empirical probes. A follow-up verification checklist
  is at the end — run it in the fork's test environment (hb-shape, hb-subset, pdftotext,
  pyftsubset are available in the sandbox).

## 1. fpdf2 TextShaping pipeline (py-pdf/fpdf2)

Pipeline, top to bottom: **bidi (`fpdf/bidi.py`, vendored UAX#9)** → **per-run HarfBuzz (`uharfbuzz`)** → **cluster→unicode map** → **per-glyph emit (subset char codes) with absolute `Tm` positioning** → **Type0/Identity-H + FontFile2 subset + `/W` + ToUnicode**.

Files: `https://raw.githubusercontent.com/py-pdf/fpdf2/master/fpdf/{fonts,bidi,output,line_break,fpdf}.py`; docs `https://py-pdf.github.io/fpdf2/TextShaping.html`; issue `https://github.com/py-pdf/fpdf2/issues/1802`.

### 1.1 Bidi — fpdf2 does NOT use python-bidi [V]
`fpdf/bidi.py` header: self-contained implementation of **Unicode Standard Annex #9, Revision 48 (Unicode 15.1.0)** incl. isolate rules (X1–X8/X6a), L1/L2 reorder, bracket-pair table from `BidiBrackets.txt`.

Paragraph split in `fpdf.py:_preload_bidirectional_text` [V]:
```python
paragraph = BidiParagraph(text=text, base_direction=paragraph_direction, preserve_bn_chars=True)
directional_segments = paragraph.get_bidi_fragments()
for bidi_text, bidi_direction in directional_segments:
    self.text_shaping["fragment_direction"] = bidi_direction
    fragments += self._preload_font_styles(bidi_text, markdown)
```
- `auto_detect_base_direction(text)` picks paragraph direction from first strong char. [V]
- `get_bidi_fragments()` → `split_bidi_fragments()` splits on embedding level; **fragments are in LOGICAL order** (L2 `reorder_resolved_levels()` is NOT applied to them). [V] So visual ordering comes from HarfBuzz per-RTL-run + emission order (see 1.4/1.5).
- Keys: `direction` (explicit rtl/ltr/None), `script`, `language`, `features` set once via `set_text_shaping(...)`. [V, TextShaping.md]

### 1.2 HarfBuzz invocation [V — `fonts.py:perform_harfbuzz_shaping`]
```python
self.hbfont.ptem = font_size_pt
buf = hb.Buffer()
buf.cluster_level = 1                      # glyph cluster = first input char → clean ToUnicode mapping
buf.add_str("".join(text))
buf.guess_segment_properties()
if "fragment_direction" in text_shaping_params ...: buf.direction = ... .value   # 'rtl'/'ltr'
if "script" ...: buf.script = ...          # e.g. 'arab', 'hebr'
if "language" ...: buf.language = ...      # e.g. 'ara', 'fa', 'he'
hb.shape(self.hbfont, buf, features)       # features dict e.g. {"kern": False}
return buf.glyph_infos, buf.glyph_positions
```
Deterministic language/script overrides are the right pattern for our Qt/HarfBuzz port (we get these from QTextLayout but must pass them explicitly for Persian, §5).

### 1.3 Shaping + cluster→Unicode mapping [V — `fonts.py:shape_text`]
- Cluster gaps (glyph infos cluster 0, 2, 3 → A,B ligated into first glyph) → that glyph must carry BOTH unicodes in the ToUnicode/CMap mapping. `get_cluster_from_text_index` via `bisect_left` over sorted cluster list.
- `gwidth = round(self.scale * self.ttfont["hmtx"].metrics[gname][0])` → **`/W` width = hmtx advance in 1000/upem**.
- `force_positioning = (gwidth != x_advance or x_offset != 0 or y_offset != 0 or y_advance != 0)`.
- `subset.pick_glyph` returns the **2-byte char code inside the subset** (used both in `Tj` and in ToUnicode). [V]

Takeaways to copy:
- **`/W` widths come from hmtx, not HB advances** [V]; GPOS kerning/mark positioning is NOT baked into widths — instead it triggers `force_positioning` → explicit `Tm` moves in the content stream (§1.4). PDF has no kerning operator for composite fonts; this is the fpdf2 answer.

### 1.4 Emission into the content stream [V — `line_break.py:render_with_text_shaping`]
```python
def adjust_pos(pos):  # font units → PDF user units
    return pos * self.font.scale * self.font_size_pt * (self.font_stretching / 100) / 1000 / self.k
for ti in self.font.shape_text(...):
    if ti["mapped_char"] is None: continue            # missing glyph
    char = self.font.escape_text(chr(ti["mapped_char"]))
    if ti["x_offset"] != 0 or ti["y_offset"] != 0:
        if text: ret += f"({text}) Tj "; text = ""
        ret += f"1 0 0 1 {offsetx * self.k:.2f} {(h - offsety) * self.k:.2f} Tm "
    text += char
    pos_x += adjust_pos(ti["x_advance"]) + char_spacing
    pos_y += adjust_pos(ti["y_advance"])
    if ti["force_positioning"] or (word_spacing and ti["mapped_char"] == space_mapped_code):
        if text: ret += f"({text}) Tj "; text = ""
        ret += f"1 0 0 1 {pos_x * self.k:.2f} {(h - pos_y) * self.k:.2f} Tm "
if text: ret += f"({text}) Tj"
```
Pattern: **accumulate adjacent glyph codes into one `(…) Tj`; break the string and issue `Tm` whenever positioning deviates** — no `TJ` arrays. Absolute runs right-anchored per run width is the robust variant for RTL.

### 1.5 Issue #1802 — the reversal rule (key verified source) [V]
> "HarfBuzz returns shaped glyphs in *visual RTL order* (rightmost glyph first in the output array, with positive `x_advance` values). … When the HarfBuzz output is written directly … via `Tj`, the first glyph … gets placed at the leftmost position instead, reversing the entire word. **For correct RTL rendering in PDF, the glyph order from HarfBuzz needs to be reversed before writing to the content stream (so the leftmost visual glyph is first in `Tj` order).**"
> "Additionally, the `ToUnicode` CMap maps each glyph independently back to its source Unicode character. Since glyphs are in visual order, the CMap entries produce characters in reversed logical order … particularly visible with the Arabic lam-alef (لا) … the two component glyphs get their Unicode mappings swapped."
> Maintainer (2026-04-06): "Text shaping doesn't apply to the `text()` method … with `cell()`, `multi_cell()` or `write()` you will get the correct result."

⚠️ **[I]** In the captured master files, `render_with_text_shaping` itself contains no `reverse()`; the reversal for RTL runs is applied elsewhere in the cell/multi_cell path — prime suspects: `fpdf.py:3924`, `fpdf.py:5042`, `fpdf.py:5218` (all `if self.text_shaping:` branches). **Do not copy this code until you confirm where the glyph order is reversed.** The rule from the issue stands regardless: HB RTL output (rightmost-first) must be reversed to leftmost-first for `Tj`. For our Qt port: shape run with `hb_buffer_set_direction(HB_DIRECTION_RTL)`, then `std::reverse` the glyph/advance arrays before emitting, and right-anchor the run's x origin (report §5.1 uses absolute positioning).

### 1.6 ToUnicode builder [V — `output.py` ~1434–1460]
```python
def format_code(unicode: int) -> str:
    if unicode > 0xFFFF:                       # astral → UTF-16BE surrogate pair
        code_high = 0xD800 | (unicode - 0x10000) >> 10
        code_low  = 0xDC00 | (unicode & 0x3FF)
        return f"{code_high:04X}{code_low:04X}"
    return f"{unicode:04X}"
# per used glyph (Type0 path): f'<{code_mapped:04X}> <{"".join(format_code(code) for code in glyph.unicode)}>\n'
# wrapped in: f"{len(bfChar)} beginbfchar\n" ... "endbfchar\n"
```
- 4 hex digits = 2-byte code space = Identity-H GID/code. [V]
- `glyph.unicode` is the **tuple from HarfBuzz clusters** (§1.3), so lam-alef → `<XXXX> <06440627>` and multi-codepoint ligatures extract to the full logical sequence. [V]
- Codes are subset-reassigned IDs — **ToUnicode is keyed to the subset's codes by construction**; exactly the "subset first, then ToUnicode from subset GIDs" rule (§3). [V]

### 1.7 Embedding container
- Type0/Identity-H + CIDFontType2 + `CIDToGIDMap /Identity` + FontDescriptor + FontFile2 (subset). [V]
- Subsetting via fontTools (`fontTools.ttLib` / `TTGlyphPen` / `varLib.instancer`). [V]

## 2. Subsetting: hb-subset vs pyftsubset, and the layout-closure pitfall
- **Both retain GSUB/GPOS by default; the killer is `--no-layout-closure`:** it prunes GSUB/GPOS rules not reachable from the retained glyph set, so Arabic joining lookups (isol→init→medi→fina chains) die and shaped text renders as isolated forms. **[I]** (flag confirmed by prior report §5.2 [V]).
- pyftsubset: default keeps layout features with layout closure. Command shape [I]: `pyftsubset font.ttf --text="<all text we will emit, incl. ZWNJ, digits, punctuation>" --layout-features='*' --glyph-names --symbol-cmap --output-file=subset.ttf`.
- hb-subset [I]: `hb-subset font.ttf --text-file=... --layout-features=*`; verify with `hb-shape subset.ttf …` that init/medi/fina/liga still fire.
- **Recommended for our use** [I, aligns with [V] report §5.2 + fpdf2]: shape all text first on the FULL font, collect the used glyph-ID set *including final forms*, then subset by glyph set with layout closure ON, then rebuild `/W` + ToUnicode from the *subset's* GIDs (order matters, §3). **Embed a shaped subset, not a unicode-only subset.**

## 3. ToUnicode CMap exact format
Canonical structure (ISO 32000-1 §9.10.3):
```
/CIDInit /ProcSet findresource begin
12 dict begin begincmap
/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def
/CMapName /Adobe-Identity-UCS def /CMapType 2 def
1 begincodespacerange <0000> <FFFF> endcodespacerange
N beginbfchar
<0041> <0041>            % single glyph → single char
<0032> <06440627>        % lam-alef ligature → TWO UTF-16BE code units (logical order!)
endbfchar
endcmap CMapName currentdict /CMap defineresource pop end end
```
- **bfchar source = the 2-byte code in the SUBSET font (Identity-H "char code")**, not original GID. [V] pitfall restated: subsetting renumbers GIDs, so building ToUnicode before subsetting from original GIDs maps to the wrong glyphs after embedding.
- **Recommended order** (== fpdf2's by construction): subset → for each emitted code:glyph → resolve glyph→cluster→logical codepoints → emit bfchar. [V]
- Astral codepoints → UTF-16BE surrogate pair in the value. [V]
- `beginbfrange` is an optimization only; `bfchar` per used code is simplest and matches fpdf2. [I]
- fpdf2 skips glyphs whose unicode tuple is empty; emits only for used glyphs. [V]

## 4. /ActualText marked content
Syntax [I — standard pattern; endorsed by prior report §5.1 step 6 citing ISO 32000 §14.9.4]:
```
BT
/F1 12 Tf
/Span << /ActualText <FEFF0633064406270645> >> BDC   % logical "سلام", UTF-16BE with FEFF BOM
  <...visual-order glyph codes...> Tj
EMC
ET
```
- `/ActualText` value = UTF-16BE **with BOM** (`FEFF` prefix), must be the **logical** string. The marked-content sequence replaces the enclosed glyphs for extraction/copy (table 321 "Actual text"). [I]
- Poppler `pdftotext` honors `/ActualText` (standard mechanism for alternate/reordered text in extractors). **[I]** — verify with the checklist.
- Use for: bidi runs (wraps the whole reversed visual run so extractors return logical order), and as the basis for our own exact search sidecar (report §5.3 path 2). Spec: 14.9.4 ISO 32000-1:2008.

## 5. Persian specifics
- **Language matters**: `hb-buffer-set-language(buf, hb_language_from_string("fa"))` selects the `FAR ` langsys of the `arab` script — switches Persian yeh (U+06CC), keheh (U+06A9) and Persian-specific forms; language `ar` will not. **[I]** — mechanism well-established; references: OpenType language-system tags `https://learn.microsoft.com/en-us/typography/opentype/spec/languagetags` (code `FAR`) and HarfBuzz arabic docs `https://harfbuzz.github.io/shaping-arabic.html`; Persian pitfall list corroborated [V] by prior report §5.4 (ZWNJ U+200C, digit sets ۰-۹/٠-٩/0-9, ی/ي/ى + ک/ك unification for search, non-joining ر ز ژ و ا د ذ, lam-alef mapping to both codepoints).
- **Vazirmatn** (`https://github.com/rastikerdar/vazirmatn`): TTF downloaded successfully [V] (122,752 B). License OFL **[I]** (verify `OFL.txt` before bundling). GSUB/GPOS coverage (Arabic joining, ZWNJ handling, Persian digits/yeh) asserted by project description [I] — verify with `hb-shape`/`ttx -t GSUB`. Comfortable bundle: Vazirmatn (Persian) + Noto Naskh Arabic + Noto Sans Hebrew.

## 6. Follow-up verification checklist (blocked by sandbox tooling; all tools present)
1. `hb-shape --font-file=/tmp/rtl/Vazirmatn-Regular.ttf --direction=rtl --script=arab --language=fa "میخواهم"` vs `--language=ar` → expect fa: yeh U+06CC/keheh U+06A9 glyphs differ from ar path (compare glyph names).
2. Run the probe script (reads strings.txt, pure-ASCII script — required because the Hermes guard crashes on non-ASCII scripts) → prints glyph/cluster/advance for سلام, میخواهم (fa vs ar), کتاب (fa vs ar), لا, שלום, fi.
3. `pyftsubset` + `hb-subset` with and without `--no-layout-closure`; `hb-shape` the subsets to confirm init/fina/medi/liga survive.
4. Generate a small fpdf2 `multi_cell` PDF with Arabic, then `pdftotext` → logical order; dump with `qpdf --qdf`/ttx to inspect Tj order, /W, ToUnicode, and the marked-content.
5. Confirm the glyph-reversal call site in fpdf2 master (`fpdf.py:5042`/`5218`/`3924`) before copying any emission code (§1.5).
