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

#include <QString>
#include <vector>

namespace pdf
{

/// Unicode normalization for RTL text search (M5).
///
/// Search must be tolerant of the ways Arabic/Persian/Hebrew text differs
/// between the PDF content stream (visual order, shaped glyphs, presentation
/// forms, ligatures) and the user's query (logical order, plain letters).
///
/// Normalization pipeline (applied to BOTH the query and the extracted text):
///   1. NFC (canonical composition; also folds presentation forms FB50-FDFF /
///      FE70-FEFF via NFKC)
///   2. Strip Arabic diacritics (tashkeel) and tatweel
///   3. Strip ZWNJ/ZWJ (U+200C/U+200D)
///   4. Unify Persian <-> Arabic letter forms (yeh, keheh, alef variants)
///   5. Unify Persian/Arabic-Indic digits with Western digits
///   6. Collapse lam-alef ligature (U+0644 U+0627) to a single lam — matches
///      PDFs whose ToUnicode degrades the ligature to lam (2-byte CMap limit)
///
/// The result is used for matching only; geometry maps back through the
/// per-character original indices returned alongside.
class PDF4QTLIBCORESHARED_EXPORT PDFRTLTextNormalizer
{
public:
    struct Options
    {
        bool stripDiacritics = true;    ///< Remove tashkeel + tatweel
        bool stripJoiners = true;       ///< Remove ZWNJ/ZWJ
        bool unifyPersianArabic = true; ///< ي<->ی, ك<->ک, أ<->ا, ة<->ه ...
        bool unifyDigits = true;        ///< ۰-۹/٠-٩ -> 0-9
        bool collapseLamAlef = true;    ///< لا -> ل (ligature degradation)
    };

    /// Normalize \p text. Returns the normalized string and (optionally) a map
    /// normalized-index -> original-index (index of the LAST original char that
    /// collapsed into the normalized char; -1 for inserted chars).
    static QString normalize(const QString& text, const Options& options, std::vector<int>* charMap = nullptr);

    /// Normalize \p text with default options.
    static QString normalize(const QString& text, std::vector<int>* charMap = nullptr);
};

} // namespace pdf
