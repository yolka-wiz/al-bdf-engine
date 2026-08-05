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

#ifndef PDFRTLTEXTENGINE_H
#define PDFRTLTEXTENGINE_H

#include "pdfglobal.h"
#include "pdfobject.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace pdf
{

/// RTL text engine: shapes Arabic/Persian/Hebrew text with HarfBuzz and emits
/// a complete PDF font (Type0/Identity-H, embedded TTF subset) plus the text
/// content stream fragment in visual order with /ActualText marked content.
///
/// Pipeline (see docs/research/002-rtl-reference-implementations.md):
///   FriBidi bidi (UAX#9) -> directional runs in LOGICAL order
///   -> HarfBuzz shapes each run (cluster_level = 1, explicit script/language)
///   -> RTL runs are REVERSED (HarfBuzz emits rightmost-first; PDF Tj needs
///      leftmost-first — the lam-alef reversal rule from fpdf2 issue #1802)
///   -> glyph codes are sequential subset IDs (Identity-H 2-byte codes)
///   -> /W advances from hmtx (1000/upem), ToUnicode from HarfBuzz clusters,
///      /ActualText wraps the run (UTF-16BE + BOM, logical order)
class PDF4QTLIBCORESHARED_EXPORT PDFRTLTextEngine
{
public:
    /// Settings for one shaped text run.
    struct Settings
    {
        QString text;            ///< Logical-order input text
        QString language;        ///< "fa", "ar", "he", ... ('' = auto)
        PDFReal fontSize = 12.0; ///< Font size in PDF points
        PDFReal x = 0.0;         ///< Run origin x (left edge of the run)
        PDFReal y = 0.0;         ///< Baseline y
        QByteArray fontData;     ///< TTF font program bytes (subsetted)
        QString fontFamily;      ///< Font family name for the descriptor
    };

    /// Result: the embedded font dictionary and the content stream fragment.
    struct Result
    {
        /// Type0 font dictionary with FontDescriptor + FontFile2 (subset).
        PDFDictionary fontDictionary;
        /// Content stream fragment (BT ... ET), visual order, with
        /// /ActualText marked content around RTL runs.
        QByteArray contentFragment;
        /// True if any run was RTL (ActualText applied).
        bool hasRTL = false;
        /// Human-readable errors (empty on success).
        QStringList errors;
    };

    /// Shapes \p settings.text and produces the embedded font + content
    /// fragment. \p fontDictionary is filled with a single font entry keyed
    /// by \p fontKey.
    static Result create(const Settings& settings, const QByteArray& fontKey);

    /// Builds the ToUnicode CMap for the subset font.
    /// \param codeToUnicode Map from subset code -> logical UTF-16BE bytes.
    static QByteArray createToUnicodeCMap(const std::vector<QPair<PDFInteger, QByteArray>>& codeToUnicode);

private:
    struct ShapedGlyph
    {
        PDFInteger code = 0;    ///< Subset code (2-byte Identity-H)
        PDFReal xAdvance = 0.0; ///< Advance in font units (hmtx)
        PDFReal xOffset = 0.0;  ///< Offset in font units (GPOS mark)
        PDFReal yOffset = 0.0;
        QByteArray unicode; ///< UTF-16BE of the cluster (logical)
    };

    struct ShapedRun
    {
        bool isRTL = false;
        size_t begin = 0; ///< Logical char range of the run (for /ActualText)
        size_t end = 0;
        std::vector<ShapedGlyph> glyphs; ///< VISUAL order (leftmost first)
        PDFReal totalAdvance = 0.0;      ///< Sum of advances, font units
    };
};

} // namespace pdf

#endif // PDFRTLTEXTENGINE_H
