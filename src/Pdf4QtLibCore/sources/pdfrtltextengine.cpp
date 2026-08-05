// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (c) 2026 albdf contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// This file is part of the albdf project, a fork of PDF4QT (MIT).
// The upstream PDF4QT portions remain under the MIT License; see the
// upstream copyright headers and the LICENSE file.

#include "pdfrtltextengine.h"

#include "pdfdocumentbuilder.h"
#include "pdfstreamfilters.h"

#include <fribidi.h>

#include <hb-ot.h>
#include <hb.h>

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
    char tag[5] = {0, 0, 0, 0, 0};
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
    if (FT_New_Memory_Face(
            library, reinterpret_cast<const FT_Byte*>(fontData.constData()), FT_Long(fontData.size()), 0, &face) != 0 ||
        !face)
    {
        FT_Done_FreeType(library);
        return metrics;
    }

    metrics.upem = face->units_per_EM > 0 ? PDFInteger(face->units_per_EM) : 1000;
    metrics.bbox =
        QRectF(face->bbox.xMin, face->bbox.yMin, face->bbox.xMax - face->bbox.xMin, face->bbox.yMax - face->bbox.yMin);
    metrics.ascent = face->ascender;
    metrics.descent = face->descender;
    metrics.ok = true;

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return metrics;
}

} // namespace

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

    hb_blob_t* blob = hb_blob_create(
        settings.fontData.constData(), unsigned(settings.fontData.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
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
    if (!fribidi_log2vis(logical.data(),
                         FriBidiStrIndex(logical.size()),
                         &baseDirection,
                         visual.data(),
                         positionsLToV.data(),
                         positionsVToL.data(),
                         levels.data()))
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
        runs.push_back(Run{i, j, rtl});
        i = j;
    }

    // ------------------------------------------------------------------
    // 3. Shape each run with HarfBuzz.
    // ------------------------------------------------------------------
    std::vector<ShapedRun> shapedRuns;
    // Per-INSTANCE code assignment (NOT GID-as-code): each glyph occurrence
    // gets a unique sequential code, so the same GID used in different
    // contexts (e.g. a fatha mark over two different base letters) maps to
    // its own ToUnicode entry. The GID for each code is recorded in
    // codeToGid and emitted as a 65536-entry /CIDToGIDMap stream.
    PDFInteger nextCode = 1;                         // code 0 = .notdef
    std::vector<PDFInteger> codeToGid;               // index = code, value = GID
    std::map<PDFInteger, QByteArray> glyphToUnicode; // code -> UTF-16BE (logical cluster)
    std::map<PDFInteger, PDFReal> glyphToWidth;      // code -> hmtx advance (font units)

    // Script selection: Arabic script for fa/ar/ur, Hebrew for he, else guess.
    hb_script_t script = HB_SCRIPT_INVALID;
    if (settings.language == QLatin1String("fa") || settings.language == QLatin1String("ar") ||
        settings.language == QLatin1String("ur"))
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
        // cluster_level = 2 (MONOTONE_CHARACTERS): every INPUT CHARACTER gets its
        // own monotone cluster value, so a combining mark (fatha etc.) carries the
        // cluster of ITS OWN char index, not its base's. This lets the ToUnicode
        // mapping give the mark its own code point (064E/064F...) instead of the
        // base letter, which is required for tashkeel-tolerant search.
        hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

        hb_buffer_add_utf16(
            buffer, textBuffer.units.data(), int(textBuffer.units.size()), int(run.begin), int(run.end - run.begin));
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

            // Unique per-instance code; the GID is stored for the
            // /CIDToGIDMap stream (code -> gid).
            const PDFInteger code = nextCode++;
            codeToGid.push_back(PDFInteger(gid));

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
            //
            // NOTE: clusters are ABSOLUTE indices into the full logical text
            // because hb_buffer_add_utf16 was called with item_offset=run.begin
            // (HarfBuzz offsets cluster values by item_offset). Do NOT add
            // run.begin again — charBegin is cluster itself. (Bug fixed in M5:
            // mixed LTR+RTL runs had run.begin > 0 and produced <0000> entries.)
            QByteArray unicode;
            const bool sharesClusterWithLeftNeighbour =
                run.isRTL ? (g > 0 && glyphInfo[g - 1].cluster == cluster)
                          : (g + 1 < glyphCount && glyphInfo[g + 1].cluster == cluster);
            if (sharesClusterWithLeftNeighbour)
            {
                // Decomposed mark: reuse the base's cluster text.
                const QByteArray& baseUnicode = (run.isRTL ? glyphClusterText[g - 1] : glyphClusterText[g + 1]);
                unicode = baseUnicode;
            }
            else
            {
                unsigned nextCluster = run.isRTL ? (g > 0 ? glyphInfo[g - 1].cluster : unsigned(run.end))
                                                 : (g + 1 < glyphCount ? glyphInfo[g + 1].cluster : unsigned(run.end));
                const size_t charBegin = size_t(cluster);
                const size_t charEnd = std::min(size_t(nextCluster), run.end);
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
            // Decide the Ts rise sign from where the mark's INK sits relative
            // to its origin (not from yOffset's sign — both fatha and kasra
            // have negative y_off but must move in opposite directions).
            // hb_font_get_glyph_extents gives ink bounds in font units with
            // the y axis pointing up: positive y_max means ink above origin.
            if (!qFuzzyIsNull(glyph.yOffset))
            {
                hb_glyph_extents_t extents = {};
                if (hb_font_get_glyph_extents(hbFont, gid, &extents) != 0)
                {
                    glyph.inkAboveOrigin = extents.y_bearing > 0;
                }
            }
            glyph.unicode = unicode;
            shapedRun.glyphs.push_back(glyph);
            shapedRun.totalAdvance += glyph.xAdvance;
        }

        // ------------------------------------------------------------------
        // RTL ordering: HarfBuzz (>= 4) emits RTL buffers in VISUAL order
        // already, starting with the LEFTMOST glyph (verified empirically with
        // hb 14.2.1: glyph[0].cluster == last logical char). PDF Tj draws the
        // first code at the current point and advances right, which matches
        // leftmost-first visual order — so NO reversal is needed. (The fpdf2
        // #1802 reversal applies to fpdf2's own buffer setup, not ours;
        // reversing here would mirror the text.)
        // ------------------------------------------------------------------

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
            content += QStringLiteral("/Span << /ActualText <%1> >> BDC\n")
                           .arg(QString::fromLatin1(actualText.toHex().toUpper()));
            result.hasRTL = true;
        }

        // Absolute position of the run's left edge.
        content += QStringLiteral("1 0 0 1 %1 %2 Tm\n").arg(runX, 0, 'f', 2).arg(settings.y, 0, 'f', 2);

        // Emit glyphs as one or more text-show operations. Kerning / mark
        // positioning from GPOS (xOffset) is folded into a numeric spacing
        // adjustment between strings — PDF's native mechanism — instead of
        // splitting the run into several Tj+repositioned Tm segments.
        // Splitting would break text extraction (PDF4QT inserts a space at Tm
        // boundaries).
        //
        // Vertical mark offsets (yOffset) cannot be expressed as TJ spacing
        // (TJ numbers are horizontal only). We use PDF text rise (`Ts`): for
        // each glyph with non-zero yOffset we flush the current hex run, emit
        // `<rise> Ts <mark> Tj 0 Ts`, and continue the run. `Ts` moves the
        // baseline up/down without moving the pen, so extraction/flow see the
        // same glyph positions (no phantom space — unlike a Tm split).
        //
        // Rise sign: HarfBuzz y_offset is the displacement of the mark's
        // ORIGIN in font units (y-up), but the mark's ink is drawn relative to
        // its origin — fatha ink sits ABOVE its origin (renders above the
        // baseline), kasra ink BELOW (renders below). PDF `Ts` positive =
        // baseline up. So the correct rise is `-yOffset * fontSize / upem`
        // when the mark's ink is above the origin, and `+yOffset * fontSize /
        // upem` when it is below. We approximate via the GPOS mark anchor the
        // same way HarfBuzz does: y_offset = baseAnchor.y - markAnchor.y; the
        // glyph's own ink position (queried once per glyph via FreeType when
        // the mark is first seen) decides the direction. Empirically verified
        // against Ghostscript renders (2026-08-05): fatha (-146) needs +Ts,
        // kasra (-335) needs -Ts.
        QString currentHex;
        QStringList spacingItems; // "<hex>" strings and numeric adjustments
        PDFReal posX = runX;
        const auto flushHex = [&spacingItems, &currentHex]() {
            if (!currentHex.isEmpty())
            {
                spacingItems << QStringLiteral("<%1>").arg(currentHex);
                currentHex.clear();
            }
        };
        // Emit the pending TJ array (if any), then reset for the next segment.
        const auto flushTj = [&content, &spacingItems]() {
            if (!spacingItems.isEmpty())
            {
                content += QStringLiteral("[%1] TJ\n").arg(spacingItems.join(QLatin1Char(' ')));
                spacingItems.clear();
            }
        };
        // (Mark ink direction was captured during shaping into
        // ShapedGlyph::inkAboveOrigin; used below for the Ts rise sign.)

        for (const ShapedGlyph& glyph : run.glyphs)
        {
            // Fold horizontal offset into the advance: TJ number is
            // (offset * 1000 / upem) * (1000 / size / 1000) adjusted —
            // PDF spec: adjustment = -(tx) * 1000 / (fontSize * hscale),
            // where tx is the desired displacement delta in text space.
            // Zero-advance glyphs (combining marks) emit INLINE with no
            // adjustment: a numeric item would create a pen gap that
            // PDF4QT's text flow heuristics convert into a phantom space
            // (gap > 1.2 * previous-advance). The mark paints over its
            // base at the current position.
            if (!qFuzzyIsNull(glyph.xOffset) && !qFuzzyIsNull(glyph.xAdvance))
            {
                flushHex();
                const PDFReal adjustment = -glyph.xOffset * 1000.0 / PDFReal(metrics.upem);
                spacingItems << QString::number(adjustment, 'f', 2);
            }
            // Vertical mark offsets: emit a per-glyph text rise so the mark
            // renders above (fatha) or below (kasra) the base letter.
            if (!qFuzzyIsNull(glyph.yOffset))
            {
                const PDFReal magnitude = std::abs(glyph.yOffset) * settings.fontSize / PDFReal(metrics.upem);
                const PDFReal rise = glyph.inkAboveOrigin ? magnitude : -magnitude;
                flushHex();
                flushTj();
                content += QStringLiteral("%1 Ts\n").arg(rise, 0, 'f', 2);
                content += QStringLiteral("<%1> Tj\n")
                               .arg(QStringLiteral("%1").arg(glyph.code, 4, 16, QLatin1Char('0')).toUpper());
                content += QStringLiteral("0 Ts\n");
                continue; // mark glyph already emitted; do not double-append
            }

            currentHex += QStringLiteral("%1").arg(glyph.code, 4, 16, QLatin1Char('0')).toUpper();
            posX += glyph.xAdvance * settings.fontSize / PDFReal(metrics.upem);
        }
        flushHex();
        flushTj();

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
    factory.beginDictionary(); // Type0
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
    factory << PDFObject::createInteger(4); // Symbolic
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
        factory << PDFObject::createStream(
            std::make_shared<PDFStream>(std::move(fontFileDict), QByteArray(compressedFont)));
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

    // /CIDToGIDMap: a 65536-entry STREAM (2 bytes per entry, big-endian,
    // entry[code] = gid, all other entries = 0). PDF 32000-1 §9.7.4.3
    // requires exactly 65536 entries (or a stream) for a 2-byte-CID font.
    // The previous short array was invalid: strict renderers (Ghostscript)
    // AND PDF4QT's own parser (pdffont.cpp reads the map only when the
    // object isStream()) ignored it and fell back to /Identity — code 1
    // painted GID 1 (.null, invisible), code 2 painted GID 2 (Latin 'A').
    // Flate-compressed like the FontFile2 stream above; the all-zero
    // padding collapses to a few hundred bytes.
    factory.beginDictionaryItem("CIDToGIDMap");
    {
        QByteArray cidToGidData(65536 * 2, '\0'); // entry 0 = 0 (.notdef)
        for (PDFInteger code = 0; code < PDFInteger(codeToGid.size()) && code + 1 <= 65535; ++code)
        {
            const PDFInteger gid = codeToGid[size_t(code)];
            const size_t offset = size_t(2 * (code + 1)); // code 0 = .notdef
            cidToGidData[offset] = char((gid >> 8) & 0xFF);
            cidToGidData[offset + 1] = char(gid & 0xFF);
        }
        const QByteArray compressedCidToGid = PDFFlateDecodeFilter::compress(cidToGidData);
        PDFDictionary cidToGidDict;
        cidToGidDict.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(compressedCidToGid.size()));
        cidToGidDict.setEntry(PDFInplaceOrMemoryString("Filter"), PDFObject::createName("FlateDecode"));
        factory << PDFObject::createStream(
            std::make_shared<PDFStream>(std::move(cidToGidDict), QByteArray(compressedCidToGid)));
    }
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
        std::make_pair(PDFInplaceOrMemoryString(fontKey), factory.takeObject())});

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

} // namespace pdf
