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

#include "pdftoolsearchtext.h"

#include "pdfdocumentreader.h"
#include "pdfoutputformatter.h"
#include "pdftextsearchengine.h"

namespace pdftool
{

static PDFToolSearchText s_searchTextApplication;

QString PDFToolSearchText::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "search-text";

    case Name:
        return PDFToolTranslationContext::tr("Search text");

    case Description:
        return PDFToolTranslationContext::tr("Search for text in the document (RTL-aware, with bounding rectangles).");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

PDFToolAbstractApplication::Options PDFToolSearchText::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | PageSelector | SearchText;
}

int PDFToolSearchText::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    if (options.searchTextQuery.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Query must not be empty."), options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFTextSearchEngine engine;
    pdf::PDFTextSearchEngine::Options searchOptions;
    searchOptions.caseSensitive = options.searchTextCaseSensitive;
    if (options.searchTextNoNormalize)
    {
        searchOptions.normalizer.stripDiacritics = false;
        searchOptions.normalizer.stripJoiners = false;
        searchOptions.normalizer.unifyPersianArabic = false;
        searchOptions.normalizer.unifyDigits = false;
        searchOptions.normalizer.collapseLamAlef = false;
    }

    QString errorMessage;
    const std::vector<pdf::PDFInteger> pages =
        options.getPageRange(pdf::PDFInteger(document.getCatalog()->getPageCount()), errorMessage, true);
    if (!errorMessage.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page range: %1").arg(errorMessage),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const pdf::PDFInteger pageFirst = pages.empty() ? 0 : pages.front();
    const pdf::PDFInteger pageLast = pages.empty() ? 0 : pages.back();

    const std::vector<pdf::PDFTextSearchEngine::Match> matches =
        engine.search(&document, options.searchTextQuery, pageFirst, pageLast, searchOptions);

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("search-results", PDFToolTranslationContext::tr("Search results"));
    formatter.endl();

    formatter.beginHeader("query", options.searchTextQuery);
    formatter.writeText("count", QString::number(static_cast<qlonglong>(matches.size())));
    formatter.endHeader();
    formatter.endl();

    formatter.beginTable("matches", PDFToolTranslationContext::tr("Matches"));
    formatter.beginTableHeaderRow("header");
    formatter.writeTableHeaderColumn("page", PDFToolTranslationContext::tr("Page"));
    formatter.writeTableHeaderColumn("item", PDFToolTranslationContext::tr("Item"));
    formatter.writeTableHeaderColumn("bbox", PDFToolTranslationContext::tr("Bounding box"));
    formatter.writeTableHeaderColumn("text", PDFToolTranslationContext::tr("Text"));
    formatter.endTableHeaderRow();

    for (const pdf::PDFTextSearchEngine::Match& match : matches)
    {
        formatter.beginTableRow("match");
        formatter.writeTableColumn("page", QString::number(match.pageIndex + 1));
        formatter.writeTableColumn("item", QString::number(static_cast<qlonglong>(match.itemIndex)));
        formatter.writeTableColumn("bbox",
                                   QStringLiteral("%1 %2 %3 %4")
                                       .arg(match.boundingRect.x())
                                       .arg(match.boundingRect.y())
                                       .arg(match.boundingRect.width())
                                       .arg(match.boundingRect.height()));
        formatter.writeTableColumn("text", match.matchedText);
        formatter.endTableRow();
    }

    formatter.endTable();
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);
    return ExitSuccess;
}

} // namespace pdftool
