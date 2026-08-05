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

#include "pdftextsearchengine.h"

#include "pdfdocument.h"
#include "pdfdocumenttextflow.h"

#include <fribidi.h>

namespace pdf
{

std::vector<PDFTextSearchEngine::Match> PDFTextSearchEngine::search(const PDFDocument* document,
                                                                    const QString& query,
                                                                    PDFInteger pageFirst,
                                                                    PDFInteger pageLast)
{
    return search(document, query, pageFirst, pageLast, Options());
}

std::vector<PDFTextSearchEngine::Match> PDFTextSearchEngine::search(const PDFDocument* document,
                                                                    const QString& query,
                                                                    PDFInteger pageFirst,
                                                                    PDFInteger pageLast,
                                                                    const Options& options)
{
    std::vector<Match> matches;

    if (!document || query.isEmpty())
    {
        return matches;
    }

    // 1. Normalize the query (logical order), then invert it to visual order
    //    so it matches the visual-order extracted text. fribidi_log2vis also
    //    SHAPES Arabic into presentation forms (FEE2/...), so normalize once
    //    more after inversion (NFKC folds presentation forms back to base).
    const QString normalizedQuery = PDFRTLTextNormalizer::normalize(query, options.normalizer);
    const QString visualQuery = PDFRTLTextNormalizer::normalize(invertToVisual(normalizedQuery), options.normalizer);
    if (visualQuery.isEmpty())
    {
        return matches;
    }

    // 2. Extract text flow per page (visual order, per-char rects).
    PDFDocumentTextFlowFactory factory;
    std::vector<PDFInteger> pageIndices;
    for (PDFInteger page = pageFirst; page <= pageLast; ++page)
    {
        pageIndices.push_back(page);
    }
    const PDFDocumentTextFlow flow =
        factory.create(document, pageIndices, PDFDocumentTextFlowFactory::Algorithm::Layout);

    for (size_t itemIndex = 0; itemIndex < flow.getSize(); ++itemIndex)
    {
        const PDFDocumentTextFlow::Item& item = *flow.getItem(itemIndex);
        if (!item.flags.testFlag(PDFDocumentTextFlow::Text) || item.text.isEmpty())
        {
            continue;
        }

        // 3. Normalize the item, keeping the original-index map for geometry.
        std::vector<int> charMap;
        const QString normalizedItem = PDFRTLTextNormalizer::normalize(item.text, options.normalizer, &charMap);

        // 4. Substring match (repeated, to find all occurrences).
        int from = 0;
        while (true)
        {
            const int matchIndex = normalizedItem.indexOf(visualQuery, from, options.caseSensitivity());
            if (matchIndex < 0)
            {
                break;
            }

            Match match;
            match.pageIndex = item.pageIndex;
            match.itemIndex = itemIndex;
            match.normalizedQueryIndex = matchIndex;

            // Map normalized range [matchIndex, matchIndex + len) back to
            // original character rects.
            const int queryLen = visualQuery.size();
            const int originalBegin = (matchIndex < static_cast<int>(charMap.size())) ? charMap.at(matchIndex) : 0;
            const int originalEnd = (matchIndex + queryLen - 1 < static_cast<int>(charMap.size()))
                                        ? charMap.at(matchIndex + queryLen - 1)
                                        : item.text.size() - 1;
            if (originalBegin >= 0 && originalEnd >= originalBegin && originalEnd < item.text.size())
            {
                match.matchedText = item.text.mid(originalBegin, originalEnd - originalBegin + 1);

                if (!item.characterBoundingRects.empty())
                {
                    QRectF unionRect;
                    for (int i = originalBegin;
                         i <= originalEnd && i < static_cast<int>(item.characterBoundingRects.size());
                         ++i)
                    {
                        unionRect = unionRect.united(item.characterBoundingRects.at(i));
                    }
                    match.boundingRect = unionRect;
                }
                else
                {
                    match.boundingRect = item.boundingRect;
                }
            }

            matches.push_back(match);
            from = matchIndex + 1;
        }
    }

    return matches;
}

QString PDFTextSearchEngine::invertToVisual(const QString& query) const
{
    if (query.isEmpty())
    {
        return QString();
    }

    // FriBidi operates on FriBidiChar (uint32). Convert from UTF-16.
    const std::vector<char16_t> units(query.utf16(), query.utf16() + query.size());
    std::vector<FriBidiChar> logical(units.size());
    for (size_t i = 0; i < units.size(); ++i)
    {
        logical[i] = static_cast<FriBidiChar>(units[i]);
    }

    std::vector<FriBidiChar> visual(units.size());
    std::vector<FriBidiStrIndex> positionsLToV(units.size());
    std::vector<FriBidiStrIndex> positionsVToL(units.size());
    std::vector<FriBidiLevel> levels(units.size());

    // Auto-detect base direction from the query content (RTL if any strong
    // RTL char is present; FriBidi's FRIBIDI_PAR_ON resolves that).
    FriBidiParType baseDir = FRIBIDI_PAR_ON;
    FriBidiLevel maxLevel = fribidi_log2vis(logical.data(),
                                            static_cast<FriBidiStrIndex>(logical.size()),
                                            &baseDir,
                                            visual.data(),
                                            positionsLToV.data(),
                                            positionsVToL.data(),
                                            levels.data());
    if (maxLevel == 0)
    {
        return query;
    }

    QString result;
    result.reserve(static_cast<int>(visual.size()));
    for (const FriBidiChar c : visual)
    {
        result.append(QChar(static_cast<ushort>(c)));
    }
    return result;
}

} // namespace pdf
