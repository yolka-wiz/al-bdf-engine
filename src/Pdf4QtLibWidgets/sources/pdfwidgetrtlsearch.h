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

#include "pdftextlayout.h"
#include "pdfwidgetsglobal.h"

#include <QString>

namespace pdf
{

class PDFDocument;
class PDFTextLayoutStorage;

/// Plain-text search for the GUI, routed through PDFTextSearchEngine (M5).
///
/// The GUI's legacy plain-text path (PDFTextLayoutStorage::find →
/// PDFTextFlow::find → QString::indexOf) performs no RTL normalization, no
/// query visual inversion and no cross-item joining, so Arabic/Persian/
/// Hebrew queries silently fail there. This adapter runs the same RTL-aware
/// engine the CLI `search-text` command uses and maps engine matches to the
/// GUI result type PDFFindResult:
///   * matched  — the engine's matchedText (visual-order text of the match)
///   * textSelectionItems — built via PDFTextLayout::createTextSelection on
///     a copy of the widget's own text layout: the highlight painter
///     (PDFTextSelectionPainter::draw) resolves PDFCharacterPointer through
///     that layout, so the pointers must reference it, not the engine flow
///   * context  — the flow text of the items covered by the match
///
/// Regex / whole-word / wildcard searches stay on the legacy path; only
/// plain-text queries are routed here.
/// \param document Document to search (must not be null)
/// \param textLayoutStorage Text layout storage of the GUI widget (must not
///        be null; the layout must be ready, i.e. the widget checked
///        PDFAsynchronousTextLayoutCompiler::isTextLayoutReady)
/// \param query Plain-text query in logical order
/// \param caseSensitivity Case sensitivity of the search
/// \param pageFirst First page (0-based, inclusive)
/// \param pageLast Last page (0-based, inclusive)
/// \returns Find results ready for the GUI result list and highlight painter
PDF4QTLIBWIDGETSSHARED_EXPORT PDFFindResults searchDocumentPlainTextRTL(const PDFDocument* document,
                                                                        const PDFTextLayoutStorage* textLayoutStorage,
                                                                        const QString& query,
                                                                        Qt::CaseSensitivity caseSensitivity,
                                                                        PDFInteger pageFirst,
                                                                        PDFInteger pageLast);

} // namespace pdf
