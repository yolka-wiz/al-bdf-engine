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

#include "pdfbidi.h"
#include "pdfdocument.h"
#include "pdfdocumenttextflow.h"

#include <algorithm>
#include <map>
#include <vector>

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

    // Extract text flow per page (visual order, per-char rects).
    PDFDocumentTextFlowFactory factory;
    std::vector<PDFInteger> pageIndices;
    for (PDFInteger page = pageFirst; page <= pageLast; ++page)
    {
        pageIndices.push_back(page);
    }
    const PDFDocumentTextFlow flow =
        factory.create(document, pageIndices, PDFDocumentTextFlowFactory::Algorithm::Layout);

    return searchFlow(flow, query, pageFirst, pageLast, options);
}

std::vector<PDFTextSearchEngine::Match> PDFTextSearchEngine::searchFlow(const PDFDocumentTextFlow& flow,
                                                                        const QString& query,
                                                                        PDFInteger pageFirst,
                                                                        PDFInteger pageLast,
                                                                        const Options& options)
{
    std::vector<Match> matches;

    if (query.isEmpty())
    {
        return matches;
    }

    // 1. Normalize the query (logical order), then invert it to visual order
    //    so it matches the visual-order extracted text. PDFBidi::logicalToVisual
    //    shapes Arabic into presentation forms (FriBidi), so normalize once
    //    more after inversion (NFKC folds presentation forms back to base).
    const QString normalizedQuery = PDFRTLTextNormalizer::normalize(query, options.normalizer);
    const QString visualQuery =
        PDFRTLTextNormalizer::normalize(PDFBidi::logicalToVisual(normalizedQuery), options.normalizer);
    if (visualQuery.isEmpty())
    {
        return matches;
    }

    // Group text items by page, preserving flow order within the page.
    std::map<PDFInteger, std::vector<size_t>> pageItems;
    for (size_t itemIndex = 0; itemIndex < flow.getSize(); ++itemIndex)
    {
        const PDFDocumentTextFlow::Item& item = *flow.getItem(itemIndex);
        if (!item.flags.testFlag(PDFDocumentTextFlow::Text) || item.text.isEmpty())
        {
            continue;
        }
        pageItems[item.pageIndex].push_back(itemIndex);
    }

    for (const auto& pageEntry : pageItems)
    {
        const PDFInteger pageIndex = pageEntry.first;
        const std::vector<size_t>& flowIndices = pageEntry.second;
        if (flowIndices.empty())
        {
            continue;
        }

        // Collect the page's text items (visual-order strings).
        struct FlowRef
        {
            size_t flowIndex = 0;
            const PDFDocumentTextFlow::Item* item = nullptr;
        };
        std::vector<FlowRef> pageRefs;
        pageRefs.reserve(flowIndices.size());
        for (const size_t idx : flowIndices)
        {
            pageRefs.push_back(FlowRef{idx, flow.getItem(idx)});
        }

        // Cluster items into lines by y-center, then sort each line by x
        // ascending. item.text is already the visual glyph string, so
        // left-to-right concatenation reconstructs the page's visual text for
        // both LTR and RTL (docstrum flow order is not guaranteed visual).
        std::sort(pageRefs.begin(), pageRefs.end(), [](const FlowRef& a, const FlowRef& b) {
            return a.item->boundingRect.center().y() < b.item->boundingRect.center().y();
        });

        std::vector<std::vector<FlowRef>> lines;
        for (const FlowRef& ref : pageRefs)
        {
            if (lines.empty())
            {
                lines.push_back({ref});
                continue;
            }
            auto& lastLine = lines.back();
            const PDFDocumentTextFlow::Item* prev = lastLine.back().item;
            const PDFDocumentTextFlow::Item* cur = ref.item;
            const bool sameLine = qAbs(cur->boundingRect.center().y() - prev->boundingRect.center().y()) <=
                                  0.5 * (cur->boundingRect.height() + prev->boundingRect.height());
            if (sameLine)
            {
                lastLine.push_back(ref);
            }
            else
            {
                lines.push_back({ref});
            }
        }

        for (auto& line : lines)
        {
            std::sort(line.begin(), line.end(), [](const FlowRef& a, const FlowRef& b) {
                return a.item->boundingRect.left() < b.item->boundingRect.left();
            });
        }

        // Build the joined string + a global origin map.
        struct Origin
        {
            size_t itemIndex = 0;
            int charIndex = -1; // -1 => separator
        };
        QString joinedText;
        std::vector<Origin> joinedMap;

        for (size_t li = 0; li < lines.size(); ++li)
        {
            if (li > 0)
            {
                // Soft boundary between lines: a small vertical gap
                // (paragraph-like leading) joins consecutive lines with a
                // word space so a phrase can span the line break; a large
                // gap (column separation) keeps the hard '\n' so far-apart
                // text never false-matches.
                const auto& prevLine = lines.at(li - 1);
                const auto& curLine = lines.at(li);
                double prevBottom = prevLine.front().item->boundingRect.bottom();
                double curTop = curLine.front().item->boundingRect.top();
                double lineHeight = 0.0;
                for (const FlowRef& ref : prevLine)
                {
                    prevBottom = qMax(prevBottom, ref.item->boundingRect.bottom());
                    lineHeight = qMax(lineHeight, ref.item->boundingRect.height());
                }
                for (const FlowRef& ref : curLine)
                {
                    curTop = qMin(curTop, ref.item->boundingRect.top());
                    lineHeight = qMax(lineHeight, ref.item->boundingRect.height());
                }
                const double vGap = curTop - prevBottom;
                joinedText += (vGap <= 0.5 * lineHeight) ? QLatin1Char(' ') : QLatin1Char('\n');
                joinedMap.push_back(Origin{0, -1});
            }
            const auto& line = lines.at(li);
            for (size_t i = 0; i < line.size(); ++i)
            {
                const FlowRef& ref = line.at(i);
                if (i > 0)
                {
                    const PDFDocumentTextFlow::Item* prev = line.at(i - 1).item;
                    const PDFDocumentTextFlow::Item* cur = ref.item;
                    const double gap = cur->boundingRect.left() - prev->boundingRect.right();
                    QString sep;
                    if (gap <= 0.0)
                    {
                        sep = QString(); // touching: mid-word continuation
                    }
                    else
                    {
                        const int nChars = qMax(1, prev->text.size() + cur->text.size());
                        const double avgCharWidth = (prev->boundingRect.width() + cur->boundingRect.width()) / nChars;
                        sep = (gap <= 2.0 * qMax(avgCharWidth, 1.0)) ? QStringLiteral(" ") : QStringLiteral("\n");
                    }
                    if (!sep.isEmpty())
                    {
                        joinedText += sep;
                        joinedMap.push_back(Origin{0, -1});
                    }
                }

                const int base = joinedText.size();
                joinedText += ref.item->text;
                for (int c = 0; c < ref.item->text.size(); ++c)
                {
                    joinedMap.push_back(Origin{ref.flowIndex, c});
                }
            }
        }

        // 3. Normalize the joined string, keeping a global char map. The
        //    joined text is VISUAL order (extracted PDF text), so enable the
        //    visual-order lam-alef collapse (the query was normalized in
        //    logical order at step 1 and inverted to visual; both sides must
        //    collapse the ligature the same way).
        std::vector<int> globalCharMap;
        PDFRTLTextNormalizer::Options flowOptions = options.normalizer;
        flowOptions.visualOrder = true;
        const QString normalizedJoined = PDFRTLTextNormalizer::normalize(joinedText, flowOptions, &globalCharMap);

        // 4. Substring match (repeated, to find all occurrences). Matches can
        //    cross soft ' ' boundaries (word/line separators) but never a hard
        //    '\n' boundary (column separation), because the query contains none.
        int from = 0;
        while (true)
        {
            const int matchIndex = normalizedJoined.indexOf(visualQuery, from, options.caseSensitivity());
            if (matchIndex < 0)
            {
                break;
            }

            Match match;
            match.pageIndex = pageIndex;
            match.normalizedQueryIndex = matchIndex;

            const int queryLen = visualQuery.size();
            const int g0 = (matchIndex < static_cast<int>(globalCharMap.size())) ? globalCharMap.at(matchIndex) : 0;
            const int g1 = (matchIndex + queryLen - 1 < static_cast<int>(globalCharMap.size()))
                               ? globalCharMap.at(matchIndex + queryLen - 1)
                               : joinedText.size() - 1;

            // Walk the global range: build spans (for geometry) and the
            // matched text (including spaces that fall inside the match).
            std::vector<ItemSpan> spans;
            QString matchedText;
            for (int g = g0; g <= g1 && g < static_cast<int>(joinedMap.size()); ++g)
            {
                const Origin& origin = joinedMap.at(static_cast<size_t>(g));
                if (origin.charIndex < 0)
                {
                    // Separator inside the match: a space is part of the phrase.
                    if (joinedText.at(g) == QLatin1Char(' '))
                    {
                        matchedText += QLatin1Char(' ');
                    }
                    continue;
                }
                if (spans.empty() || spans.back().itemIndex != origin.itemIndex)
                {
                    spans.push_back(ItemSpan{origin.itemIndex, origin.charIndex, origin.charIndex});
                }
                else
                {
                    spans.back().charEnd = origin.charIndex;
                }
                matchedText += flow.getItem(origin.itemIndex)->text.at(origin.charIndex);
            }

            // Union the character rects across the covered spans.
            QRectF spanUnion;
            for (const ItemSpan& span : spans)
            {
                const PDFDocumentTextFlow::Item* item = flow.getItem(span.itemIndex);
                if (!item->characterBoundingRects.empty())
                {
                    for (int i = span.charBegin;
                         i <= span.charEnd && i < static_cast<int>(item->characterBoundingRects.size());
                         ++i)
                    {
                        spanUnion = spanUnion.united(item->characterBoundingRects.at(static_cast<size_t>(i)));
                    }
                }
                else
                {
                    spanUnion = spanUnion.united(item->boundingRect);
                }
            }

            if (!spans.empty())
            {
                match.itemIndex = spans.front().itemIndex;
                match.matchedText = matchedText;
                match.boundingRect = spanUnion;
                match.spans = std::move(spans);
                matches.push_back(match);
            }
            from = matchIndex + 1;
        }
    }

    return matches;
}

} // namespace pdf
