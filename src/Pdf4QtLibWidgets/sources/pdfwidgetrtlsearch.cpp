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

#include "pdfwidgetrtlsearch.h"

#include "pdfdocument.h"
#include "pdfdocumenttextflow.h"
#include "pdftextsearchengine.h"

#include <utility>
#include <vector>

namespace pdf
{

namespace
{
/// Builds the context string for a match: the flow text of the items covered
/// by the match (visual order, trimmed). This mirrors the line context the
/// legacy path shows in the results table, but comes from the engine flow.
QString buildMatchContext(const PDFDocumentTextFlow& flow, const PDFTextSearchEngine::Match& match)
{
    QString context;
    for (const PDFTextSearchEngine::ItemSpan& span : match.spans)
    {
        const PDFDocumentTextFlow::Item* item = flow.getItem(span.itemIndex);
        if (!item)
        {
            continue;
        }
        if (!context.isEmpty())
        {
            context += QLatin1Char(' ');
        }
        context += item->text;
    }
    return context.trimmed();
}
} // namespace

PDFFindResults searchDocumentPlainTextRTL(const PDFDocument* document,
                                          const PDFTextLayoutStorage* textLayoutStorage,
                                          const QString& query,
                                          Qt::CaseSensitivity caseSensitivity,
                                          PDFInteger pageFirst,
                                          PDFInteger pageLast)
{
    PDFFindResults results;

    if (!document || !textLayoutStorage || query.isEmpty() || pageLast < pageFirst)
    {
        return results;
    }

    // Extract the document text flow once: the engine matches against it and
    // we reuse it to build the result contexts. Algorithm::Layout is exactly
    // what PDFTextSearchEngine::search uses (and what the CLI search-text
    // command runs), so GUI results agree with CLI results.
    std::vector<PDFInteger> pageIndices;
    pageIndices.reserve(static_cast<size_t>(pageLast - pageFirst + 1));
    for (PDFInteger page = pageFirst; page <= pageLast; ++page)
    {
        pageIndices.push_back(page);
    }

    PDFDocumentTextFlowFactory factory;
    const PDFDocumentTextFlow flow =
        factory.create(document, pageIndices, PDFDocumentTextFlowFactory::Algorithm::Layout);

    PDFTextSearchEngine::Options options;
    options.caseSensitive = (caseSensitivity == Qt::CaseSensitive);

    PDFTextSearchEngine engine;
    const std::vector<PDFTextSearchEngine::Match> matches =
        engine.searchFlow(flow, query, pageFirst, pageLast, options);

    results.reserve(matches.size());
    for (const PDFTextSearchEngine::Match& match : matches)
    {
        PDFFindResult findResult;
        findResult.matched = match.matchedText;
        findResult.context = buildMatchContext(flow, match);

        // The highlight painter resolves PDFCharacterPointer through the
        // widget's own text layout (PDFTextSelectionPainter::draw), so the
        // selection items must reference that layout, not the engine flow.
        // createTextSelection is geometric (nearest character right of
        // point 1 / left of point 2) and is not thread-safe (it mutates the
        // layout), so operate on a copy of the storage layout for the page —
        // PDFTextLayoutStorage::getTextLayout returns a copy by value.
        PDFTextLayout pageLayout = textLayoutStorage->getTextLayout(match.pageIndex);
        PDFTextSelection selection = pageLayout.createTextSelection(
            match.pageIndex, match.boundingRect.topLeft(), match.boundingRect.bottomRight());
        for (auto it = selection.begin(); it != selection.end(); ++it)
        {
            findResult.textSelectionItems.emplace_back(it->start, it->end);
        }

        // A result without selection items cannot be rendered, and would
        // break PDFFindResult::operator< (the widget's sort reads
        // textSelectionItems.front()), so keep only renderable results.
        if (!findResult.textSelectionItems.empty())
        {
            results.push_back(std::move(findResult));
        }
    }

    return results;
}

} // namespace pdf
