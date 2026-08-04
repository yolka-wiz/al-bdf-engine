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
