// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (c) 2026 pdfedit contributors
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
// This file is part of the pdfedit project, a fork of PDF4QT (MIT).
// The upstream PDF4QT portions remain under the MIT License; see the
// upstream copyright headers and the LICENSE file.

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
///   7. Collapse duplicate yeh (U+06CC/U+064A) produced by the decomposed-mark
///      artifact (HarfBuzz splits yeh into base + dot; both map to the same
///      ToUnicode char) — see P2 in docs/PROBLEMS.md
///
/// The result is used for matching only; geometry maps back through the
/// per-character original indices returned alongside.
class PDF4QTLIBCORESHARED_EXPORT PDFRTLTextNormalizer
{
public:
    struct Options
    {
        bool stripDiacritics = true;      ///< Remove tashkeel + tatweel
        bool stripJoiners = true;         ///< Remove ZWNJ/ZWJ
        bool unifyPersianArabic = true;   ///< ي<->ی, ك<->ک, أ<->ا, ة<->ه ...
        bool unifyDigits = true;          ///< ۰-۹/٠-٩ -> 0-9
        bool collapseLamAlef = true;      ///< لا -> ل (ligature degradation)
        bool collapseDuplicateYeh = true; ///< یی -> ی (decomposed-mark artifact)
    };

    /// Normalize \p text. Returns the normalized string and (optionally) a map
    /// normalized-index -> original-index (index of the LAST original char that
    /// collapsed into the normalized char; -1 for inserted chars).
    static QString normalize(const QString& text, const Options& options, std::vector<int>* charMap = nullptr);

    /// Normalize \p text with default options.
    static QString normalize(const QString& text, std::vector<int>* charMap = nullptr);
};

} // namespace pdf
