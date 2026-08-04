// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfrtltextengine.h"

#include "pdfdocumentbuilder.h"
#include "pdfstreamfilters.h"

#include <fribidi.h>

#include <hb.h>
#include <hb-ot.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <QByteArray>
#include <QStringList>

#include <cmath>
#include <map>

namespace pdf
{

namespace
{

/// Converts a QString to a UTF-16 buffer for HarfBuzz.
struct Utf16Buffer
{
    std::vector<uint16_t> units;
};

Utf16Buffer toUtf16Buffer(const QString& text)
{
    Utf16Buffer buffer;
    buffer.units.reserve(text.size());
    for (QChar ch : text)
    {
        buffer.units.push_back(ch.unicode());
    }
    return buffer;
}

/// Maps a HarfBuzz script tag to a human name (for the font descriptor).
QByteArray scriptToTagString(hb_script_t script)
{
    char tag[5] = { 0, 0, 0, 0, 0 };
    hb_tag_to_string(script, tag);
    return QByteArray(tag, 4);
}

/// Reads font metrics via FreeType (bbox, ascent, descent, upem).
struct FontMetrics
{
    PDFInteger upem = 1000;
    QRectF bbox;
    PDFReal ascent = 0.0;
    PDFReal descent = 0.0;
    bool ok = false;
};

FontMetrics readFontMetrics(const QByteArray& fontData)
{
    FontMetrics metrics;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&library) != 0)
    {
        return metrics;
    }
    if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(fontData.constData()), FT_Long(fontData.size()), 0, &face) != 0 || !face)
    {
        FT_Done_FreeType(library);
        return metrics;
    }

    metrics.upem = face->units_per_EM > 0 ? PDFInteger(face->units_per_EM) : 1000;
    metrics.bbox = QRectF(face->bbox.xMin, face->bbox.yMin, face->bbox.xMax - face->bbox.xMin, face->bbox.yMax - face->bbox.yMin);
    metrics.ascent = face->ascender;
    metrics.descent = face->descender;
    metrics.ok = true;

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return metrics;
}

}   // namespace

PDFRTLTextEngine::Result PDFRTLTextEngine::create(const Settings& settings, const QByteArray& fontKey)
{
    Result result;

    // ------------------------------------------------------------------
    // 0. Font metrics (FreeType) + HarfBuzz face/font.
    // ------------------------------------------------------------------
    const FontMetrics metrics = readFontMetrics(settings.fontData);
    if (!metrics.ok)
    {
        result.errors << QStringLiteral("Cannot read font data.");
        return result;
    }

    hb_blob_t* blob = hb_blob_create(settings.fontData.constData(), unsigned(settings.fontData.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    hb_face_t* face = hb_face_create(blob, 0);
    hb_font_t* hbFont = hb_font_create(face);
    hb_ot_font_set_funcs(hbFont);
    hb_font_set_scale(hbFont, metrics.upem, metrics.upem);

    // ------------------------------------------------------------------
    // 1. FriBidi: logical text -> embedding levels.
    //    We keep the LOGICAL order for shaping (per run), and derive runs
    //    from level parity: odd level = RTL run, even level = LTR run.
    // ------------------------------------------------------------------
    const Utf16Buffer textBuffer = toUtf16Buffer(settings.text);
    std::vector<FriBidiChar> logical(textBuffer.units.size());
    std::vector<FriBidiChar> visual(textBuffer.units.size());
    std::vector<FriBidiStrIndex> positionsLToV(textBuffer.units.size());
    std::vector<FriBidiStrIndex> positionsVToL(textBuffer.units.size());
    std::vector<FriBidiLevel> levels(textBuffer.units.size());
    for (size_t i = 0; i < textBuffer.units.size(); ++i)
    {
        logical[i] = textBuffer.units[i];
    }

    FriBidiParType baseDirection = FRIBIDI_PAR_LTR;
    if (settings.language == QLatin1String("fa") || settings.language == QLatin1String("ar") ||
        settings.language == QLatin1String("he") || settings.language == QLatin1String("ur"))
    {
        baseDirection = FRIBIDI_PAR_RTL;
    }

    FriBidiLevel maxLevel = 0;
    if (!fribidi_log2vis(logical.data(), FriBidiStrIndex(logical.size()), &baseDirection, visual.data(),
                         positionsLToV.data(), positionsVToL.data(), levels.data()))
    {
        result.errors << QStringLiteral("FriBidi failed.");
        hb_font_destroy(hbFont);
        hb_face_destroy(face);
        hb_blob_destroy(blob);
        return result;
    }

    // ------------------------------------------------------------------
    // 2. Split into maximal directional runs (logical order substrings).
    // ------------------------------------------------------------------
    struct Run
    {
        size_t begin = 0;
        size_t end = 0;
        bool isRTL = false;
    };
    std::vector<Run> runs;
    for (size_t i = 0; i < levels.size();)
    {
        const bool rtl = (levels[i] & 1) != 0;
        size_t j = i + 1;
        while (j < levels.size() && ((levels[j] & 1) != 0) == rtl)
        {
            ++j;
        }
        runs.push_back(Run{ i, j, rtl });
        i = j;
    }

    // ------------------------------------------------------------------
    // 3. Shape each run with HarfBuzz.
    // ------------------------------------------------------------------
    std::vector<ShapedRun> shapedRuns;
    std::map<PDFInteger, QByteArray> glyphToUnicode;  // GID -> UTF-16BE (logical cluster)
    std::map<PDFInteger, PDFReal> glyphToWidth;       // GID -> hmtx advance (font units)

    // Script selection: Arabic script for fa/ar/ur, Hebrew for he, else guess.
    hb_script_t script = HB_SCRIPT_INVALID;
    if (settings.language == QLatin1String("fa") || settings.language == QLatin1String("ar") || settings.language == QLatin1String("ur"))
    {
        script = HB_SCRIPT_ARABIC;
    }
    else if (settings.language == QLatin1String("he"))
    {
        script = HB_SCRIPT_HEBREW;
    }
    else
    {
        script = hb_script_from_string("Arab", -1); // default assumption for RTL engine
    }

    hb_language_t language = hb_language_from_string(settings.language.toUtf8().constData(), -1);

    for (const Run& run : runs)
    {
        if (run.begin == run.end)
        {
            continue;
        }

        ShapedRun shapedRun;
        shapedRun.isRTL = run.isRTL;
        shapedRun.begin = run.begin;
        shapedRun.end = run.end;

        hb_buffer_t* buffer = hb_buffer_create();
        hb_buffer_set_direction(buffer, run.isRTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, script);
        if (language)
        {
            hb_buffer_set_language(buffer, language);
        }
        // cluster_level = 1 (MONOTONE_GRAPHEMES): a glyph's cluster is the
        // first input char index — clean ToUnicode mapping (fpdf2 pattern).
        hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);

        hb_buffer_add_utf16(buffer, textBuffer.units.data(), int(textBuffer.units.size()),
                            int(run.begin), int(run.end - run.begin));
        hb_shape(hbFont, buffer, nullptr, 0);

        unsigned glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, &glyphCount);

        // Per-glyph full cluster text (UTF-16BE), kept for mark-glyph reuse.
        std::vector<QByteArray> glyphClusterText;
        glyphClusterText.reserve(glyphCount);

        for (unsigned g = 0; g < glyphCount; ++g)
        {
            const hb_codepoint_t gid = glyphInfo[g].codepoint;
            const unsigned cluster = glyphInfo[g].cluster;

            // The code in Identity-H is the GID itself (full-font embedding;
            // no subset renumbering, so /CIDToGIDMap /Identity is exact).
            const PDFInteger code = PDFInteger(gid);

            // /W advance: hmtx advance (1000/upem) — nominal, not shaped.
            const PDFReal hmtxAdvance = hb_font_get_glyph_h_advance(hbFont, gid);
            glyphToWidth[code] = hmtxAdvance;

            // Cluster -> logical code points (UTF-16BE) for ToUnicode.
            // HarfBuzz emits glyphs in VISUAL order. For RTL runs the clusters
            // DECREASE along the buffer (rightmost glyph first, highest cluster
            // first); for LTR they increase. A glyph covers the logical chars
            // [cluster[g], boundary) where boundary is the neighbouring glyph's
            // cluster on the visual-left side (which in buffer order is g-1 for
            // RTL, g+1 for LTR) or the run end at the visual left edge.
            // Glyphs that share a cluster with their visual-left neighbour (a
            // base letter decomposed into base + combining mark, e.g. Arabic
            // yeh = base + dots) map to the SAME unicode as that neighbour.
            QByteArray unicode;
            const bool sharesClusterWithLeftNeighbour = run.isRTL
                ? (g > 0 && glyphInfo[g - 1].cluster == cluster)
                : (g + 1 < glyphCount && glyphInfo[g + 1].cluster == cluster);
            if (sharesClusterWithLeftNeighbour)
            {
                // Decomposed mark: reuse the base's cluster text.
                const QByteArray& baseUnicode = (run.isRTL ? glyphClusterText[g - 1] : glyphClusterText[g + 1]);
                unicode = baseUnicode;
            }
            else
            {
                unsigned nextCluster = run.isRTL
                    ? (g > 0 ? glyphInfo[g - 1].cluster : unsigned(run.end - run.begin))
                    : (g + 1 < glyphCount ? glyphInfo[g + 1].cluster : unsigned(run.end - run.begin));
                const size_t charBegin = size_t(run.begin) + cluster;
                const size_t charEnd = std::min(size_t(run.begin) + nextCluster, run.end);
                for (size_t ci = charBegin; ci < charEnd; ++ci)
                {
                    const char32_t cp = char32_t(textBuffer.units[ci]);
                    // UTF-16BE encoding of the code point.
                    if (cp <= 0xFFFF)
                    {
                        unicode.append(char(0xFF & (cp >> 8)));
                        unicode.append(char(0xFF & cp));
                    }
                    else
                    {
                        const char32_t v = cp - 0x10000;
                        const char16_t hi = char16_t(0xD800 + (v >> 10));
                        const char16_t lo = char16_t(0xDC00 + (v & 0x3FF));
                        unicode.append(char(0xFF & (hi >> 8)));
                        unicode.append(char(0xFF & hi));
                        unicode.append(char(0xFF & (lo >> 8)));
                        unicode.append(char(0xFF & lo));
                    }
                }
            }
            if (unicode.isEmpty())
            {
                // No character mapping (e.g. a control glyph): keep a zero
                // entry so ToUnicode still has a mapping.
                unicode.append(char(0x00));
                unicode.append(char(0x00));
            }
            // ToUnicode CMap entries must be EXACTLY 2 bytes (one UTF-16BE
            // code unit): PDF4QT's CMap parser (fetchUnicode) rejects longer
            // destinations and maps them to 0. Ligatures therefore degrade to
            // their first char here; full logical text is carried separately
            // by the /ActualText marked content in the content stream.
            QByteArray toUnicodeEntry = unicode;
            if (toUnicodeEntry.size() > 2)
            {
                toUnicodeEntry = toUnicodeEntry.left(2);
            }
            glyphToUnicode[code] = toUnicodeEntry;
            glyphClusterText.push_back(unicode);

            ShapedGlyph glyph;
            glyph.code = code;
            glyph.xAdvance = PDFReal(hmtxAdvance);
            glyph.xOffset = PDFReal(glyphPos[g].x_offset);
            glyph.yOffset = PDFReal(glyphPos[g].y_offset);
            glyph.unicode = unicode;
            shapedRun.glyphs.push_back(glyph);
            shapedRun.totalAdvance += glyph.xAdvance;
        }

        // ------------------------------------------------------------------
        // 4. THE REVERSAL RULE (fpdf2 issue #1802): HarfBuzz emits RTL runs
        //    in visual order (rightmost glyph first). PDF Tj places the first
        //    code at the leftmost position, which would mirror the word.
        //    Reverse to leftmost-first for correct rendering.
        // ------------------------------------------------------------------
        if (run.isRTL)
        {
            std::reverse(shapedRun.glyphs.begin(), shapedRun.glyphs.end());
        }

        hb_buffer_destroy(buffer);
        shapedRuns.push_back(std::move(shapedRun));
    }

    hb_font_destroy(hbFont);
    hb_face_destroy(face);
    hb_blob_destroy(blob);

    // ------------------------------------------------------------------
    // 5. Build the content stream fragment: BT ... ET, one Tj per run,
    //    absolute Tm positioning, /ActualText around RTL runs.
    // ------------------------------------------------------------------
    QString content;
    content += QStringLiteral("BT\n");

    // Font key + size.
    content += QStringLiteral("/%1 %2 Tf\n").arg(QString::fromLatin1(fontKey)).arg(settings.fontSize);

    PDFReal cursorX = settings.x;

    for (const ShapedRun& run : shapedRuns)
    {
        const PDFReal runWidth = run.totalAdvance * settings.fontSize / PDFReal(metrics.upem);
        PDFReal runX = cursorX;

        // Right-anchor RTL runs: the run's visual left edge is the origin.
        // Since we reversed the glyphs (leftmost first), the first glyph is
        // at runX and advances increase x — so no extra anchoring needed.

        if (run.isRTL)
        {
            // /ActualText: logical string, UTF-16BE with BOM.
            QByteArray actualText;
            actualText.append(char(0xFE));
            actualText.append(char(0xFF));
            for (size_t ci = run.begin; ci < run.end; ++ci)
            {
                const char32_t cp = char32_t(textBuffer.units[ci]);
                if (cp <= 0xFFFF)
                {
                    actualText.append(char(0xFF & (cp >> 8)));
                    actualText.append(char(0xFF & cp));
                }
                else
                {
                    const char32_t v = cp - 0x10000;
                    const char16_t hi = char16_t(0xD800 + (v >> 10));
                    const char16_t lo = char16_t(0xDC00 + (v & 0x3FF));
                    actualText.append(char(0xFF & (hi >> 8)));
                    actualText.append(char(0xFF & hi));
                    actualText.append(char(0xFF & (lo >> 8)));
                    actualText.append(char(0xFF & lo));
                }
            }
            content += QStringLiteral("/Span << /ActualText <%1> >> BDC\n").arg(QString::fromLatin1(actualText.toHex().toUpper()));
            result.hasRTL = true;
        }

        // Absolute position of the run's left edge.
        content += QStringLiteral("1 0 0 1 %1 %2 Tm\n").arg(runX, 0, 'f', 2).arg(settings.y, 0, 'f', 2);

        // Emit glyphs. Accumulate hex codes into one Tj; when a glyph has
        // GPOS offsets (marks, kerning), flush and reposition (fpdf2's
        // force_positioning pattern).
        QString hexAccum;
        PDFReal posX = runX;
        const auto flush = [&content, &hexAccum]()
        {
            if (!hexAccum.isEmpty())
            {
                content += QStringLiteral("<%1> Tj\n").arg(hexAccum);
                hexAccum.clear();
            }
        };

        for (const ShapedGlyph& glyph : run.glyphs)
        {
            const bool needsPositioning = !qFuzzyIsNull(glyph.xOffset) || !qFuzzyIsNull(glyph.yOffset);
            if (needsPositioning)
            {
                flush();
                const PDFReal gx = runX + (posX - runX) + glyph.xOffset * settings.fontSize / PDFReal(metrics.upem);
                const PDFReal gy = settings.y + glyph.yOffset * settings.fontSize / PDFReal(metrics.upem);
                content += QStringLiteral("1 0 0 1 %1 %2 Tm\n").arg(gx, 0, 'f', 2).arg(gy, 0, 'f', 2);
            }
            hexAccum += QStringLiteral("%1").arg(glyph.code, 4, 16, QLatin1Char('0')).toUpper();
            posX += glyph.xAdvance * settings.fontSize / PDFReal(metrics.upem);
        }
        flush();

        if (run.isRTL)
        {
            content += QStringLiteral("EMC\n");
        }

        cursorX += runWidth;
    }

    content += QStringLiteral("ET\n");
    result.contentFragment = content.toUtf8();

    // ------------------------------------------------------------------
    // 6. Build the Type0 font dictionary with the embedded font program.
    // ------------------------------------------------------------------
    PDFObjectFactory factory;
    factory.beginDictionary();                                  // Type0
    factory.beginDictionaryItem("Type");
    factory << PDFObject::createName("Font");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << PDFObject::createName("Type0");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BaseFont");
    factory << PDFObject::createName((settings.fontFamily + QStringLiteral("-Identity")).toLatin1());
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Encoding");
    factory << PDFObject::createName("Identity-H");
    factory.endDictionaryItem();

    // Descendant font: CIDFontType2.
    factory.beginDictionaryItem("DescendantFonts");
    factory.beginArray();
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << PDFObject::createName("Font");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << PDFObject::createName("CIDFontType2");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BaseFont");
    factory << PDFObject::createName((settings.fontFamily + QStringLiteral("-Identity")).toLatin1());
    factory.endDictionaryItem();
    factory.beginDictionaryItem("CIDSystemInfo");
    factory.beginDictionary();
    factory.beginDictionaryItem("Registry");
    factory << PDFObject::createString("Adobe");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Ordering");
    factory << PDFObject::createString("Identity");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Supplement");
    factory << PDFObject::createInteger(0);
    factory.endDictionaryItem();
    factory.endDictionary();
    factory.endDictionaryItem();

    // FontDescriptor with FontFile2 (embedded font program).
    factory.beginDictionaryItem("FontDescriptor");
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << PDFObject::createName("FontDescriptor");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("FontName");
    factory << PDFObject::createName((settings.fontFamily + QStringLiteral("-Identity")).toLatin1());
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Flags");
    factory << PDFObject::createInteger(4);  // Symbolic
    factory.endDictionaryItem();
    factory.beginDictionaryItem("FontBBox");
    factory.beginArray();
    factory << PDFObject::createReal(metrics.bbox.left());
    factory << PDFObject::createReal(metrics.bbox.top());
    factory << PDFObject::createReal(metrics.bbox.right());
    factory << PDFObject::createReal(metrics.bbox.bottom());
    factory.endArray();
    factory.endDictionaryItem();
    factory.beginDictionaryItem("ItalicAngle");
    factory << PDFObject::createInteger(0);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Ascent");
    factory << PDFObject::createReal(metrics.ascent);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Descent");
    factory << PDFObject::createReal(metrics.descent);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("CapHeight");
    factory << PDFObject::createReal(metrics.ascent);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("StemV");
    factory << PDFObject::createInteger(80);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("FontFile2");
    {
        const QByteArray compressedFont = PDFFlateDecodeFilter::compress(settings.fontData);
        PDFDictionary fontFileDict;
        fontFileDict.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(compressedFont.size()));
        fontFileDict.setEntry(PDFInplaceOrMemoryString("Filter"), PDFObject::createName("FlateDecode"));
        factory << PDFObject::createStream(std::make_shared<PDFStream>(std::move(fontFileDict), QByteArray(compressedFont)));
    }
    factory.endDictionaryItem();
    factory.endDictionary();
    factory.endDictionaryItem();

    // /W widths (hmtx advances, 1000/upem per PDF spec). Sparse form:
    // [c1 [w1] c2 [w2] ...] — each CID followed by a one-element width array
    // (the parser reads startCID then arrayOrCID; an int would be treated as
    // an end CID for the range form).
    factory.beginDictionaryItem("W");
    factory.beginArray();
    for (const auto& [code, width] : glyphToWidth)
    {
        const PDFInteger width1000 = PDFInteger(std::llround(width * 1000.0 / PDFReal(metrics.upem)));
        factory << PDFObject::createInteger(code);
        factory.beginArray();
        factory << PDFObject::createInteger(width1000);
        factory.endArray();
    }
    factory.endArray();
    factory.endDictionaryItem();

    // /CIDToGIDMap: Identity (code == GID with full embedding).
    factory.beginDictionaryItem("CIDToGIDMap");
    factory << PDFObject::createName("Identity");
    factory.endDictionaryItem();
    factory.endDictionary();
    factory.endArray();
    factory.endDictionaryItem();

    // ToUnicode CMap stream.
    factory.beginDictionaryItem("ToUnicode");
    {
        const std::vector<QPair<PDFInteger, QByteArray>> codeToUnicode(glyphToUnicode.begin(), glyphToUnicode.end());
        const QByteArray cmapData = createToUnicodeCMap(codeToUnicode);
        PDFDictionary cmapDict;
        cmapDict.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(cmapData.size()));
        factory << PDFObject::createStream(std::make_shared<PDFStream>(std::move(cmapDict), QByteArray(cmapData)));
    }
    factory.endDictionaryItem();

    factory.endDictionary();

    result.fontDictionary = PDFDictionary(std::vector<PDFDictionary::DictionaryEntry>{
        std::make_pair(PDFInplaceOrMemoryString(fontKey), factory.takeObject())
    });

    return result;
}

QByteArray PDFRTLTextEngine::createToUnicodeCMap(const std::vector<QPair<PDFInteger, QByteArray>>& codeToUnicode)
{
    QString cmap;
    cmap += QStringLiteral("/CIDInit /ProcSet findresource begin\n");
    cmap += QStringLiteral("12 dict begin begincmap\n");
    cmap += QStringLiteral("/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n");
    cmap += QStringLiteral("/CMapName /Adobe-Identity-UCS def /CMapType 2 def\n");
    cmap += QStringLiteral("1 begincodespacerange <0000> <FFFF> endcodespacerange\n");

    QString bfchar;
    int count = 0;
    for (const auto& [code, unicode] : codeToUnicode)
    {
        if (unicode.size() < 2)
        {
            continue;
        }
        bfchar += QStringLiteral("<%1> <%2>\n")
                          .arg(code, 4, 16, QLatin1Char('0'))
                          .arg(QString::fromLatin1(unicode.toHex().toUpper()));
        ++count;
    }
    cmap += QStringLiteral("%1 beginbfchar\n%2endbfchar\n").arg(count).arg(bfchar);
    cmap += QStringLiteral("endcmap CMapName currentdict /CMap defineresource pop end end\n");
    return cmap.toUtf8();
}

}   // namespace pdf
