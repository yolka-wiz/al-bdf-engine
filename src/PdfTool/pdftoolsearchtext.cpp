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
    const std::vector<pdf::PDFInteger> pages = options.getPageRange(
        pdf::PDFInteger(document.getCatalog()->getPageCount()), errorMessage, true);
    if (!errorMessage.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page range: %1").arg(errorMessage), options.outputCodec);
        return ErrorInvalidArguments;
    }
    const pdf::PDFInteger pageFirst = pages.empty() ? 0 : pages.front();
    const pdf::PDFInteger pageLast = pages.empty() ? 0 : pages.back();

    const std::vector<pdf::PDFTextSearchEngine::Match> matches = engine.search(
        &document,
        options.searchTextQuery,
        pageFirst,
        pageLast,
        searchOptions);

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
        formatter.writeTableColumn("bbox", QStringLiteral("%1 %2 %3 %4")
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

}   // namespace pdftool
