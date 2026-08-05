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

#pragma once

#include "pdfobject.h"
#include "pdfrtltextnormalizer.h"

#include <QRectF>
#include <QString>
#include <vector>

namespace pdf
{

class PDFDocument;

/// Text search engine with RTL support (M5).
///
/// PDF content streams store text in VISUAL order; the user's query is in
/// LOGICAL order. The engine:
///   1. Extracts the per-page text flow (visual order, per-character rects)
///   2. Normalizes each item (PDFRTLTextNormalizer)
///   3. Inverts the normalized query to visual order via FriBidi
///      (fribidi_log2vis) so it matches the extracted text
///   4. Performs case-insensitive substring matching and maps matched
///      normalized indices back to original character bounding rects
///
/// Matching happens within a single text item; matches spanning multiple
/// items are reported separately (v1 limitation).
class PDF4QTLIBCORESHARED_EXPORT PDFTextSearchEngine
{
public:
    struct Match
    {
        PDFInteger pageIndex = 0;     ///< 0-based page
        size_t itemIndex = 0;         ///< text-flow item index on the page
        QString matchedText;          ///< original (un-normalized) text
        QRectF boundingRect;          ///< union of matched char rects (page coords)
        int normalizedQueryIndex = 0; ///< position in the visual query string
    };

    struct Options
    {
        PDFRTLTextNormalizer::Options normalizer;
        bool caseSensitive = false;
        Qt::CaseSensitivity caseSensitivity() const
        {
            return caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        }
    };

    /// Search \p query in pages [pageFirst, pageLast] (0-based, inclusive).
    std::vector<Match> search(const PDFDocument* document,
                              const QString& query,
                              PDFInteger pageFirst,
                              PDFInteger pageLast,
                              const Options& options);

    /// Search with default options.
    std::vector<Match>
    search(const PDFDocument* document, const QString& query, PDFInteger pageFirst, PDFInteger pageLast);

private:
    /// Invert a logical query to visual order using FriBidi.
    QString invertToVisual(const QString& query) const;
};

} // namespace pdf
