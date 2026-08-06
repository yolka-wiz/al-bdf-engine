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

#include "pdftooldeletepage.h"

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfexception.h"
#include "pdfoptimizer.h"
#include "pdfutils.h"

namespace pdftool
{

static PDFToolDeletePage s_deletePageApplication;

QString PDFToolDeletePage::getStandardString(PDFToolAbstractApplication::StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "delete-page";

    case Name:
        return PDFToolTranslationContext::tr("Delete pages");

    case Description:
        return PDFToolTranslationContext::tr("Delete page(s) from a document.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

int PDFToolDeletePage::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    // The second positional argument is the output document.
    if (options.deletePageOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const QString outputDocument = options.deletePageOutputDocument;

    if (options.deletePagePages.isEmpty() && options.deletePageSelection.isEmpty())
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Page number (--page) or page selection (--page-select) must be specified."),
            options.outputCodec);
        return ErrorInvalidArguments;
    }

    const pdf::PDFInteger pageCount = document.getCatalog()->getPageCount();

    // Parse the selection (1-based page numbers, e.g. '1,3' or '2-4') with the
    // same interval parser the PageSelector option uses.
    QStringList parts = options.deletePagePages;
    if (!options.deletePageSelection.isEmpty())
    {
        parts << options.deletePageSelection;
    }

    QString parseError;
    pdf::PDFClosedIntervalSet selection = pdf::PDFClosedIntervalSet::parse(1, pageCount, parts.join(','), &parseError);
    if (!parseError.isEmpty())
    {
        PDFConsole::writeError(parseError, options.outputCodec);
        return ErrorInvalidArguments;
    }

    std::vector<pdf::PDFInteger> pagesToDelete = selection.unfold(); // 1-based, sorted, unique
    for (const pdf::PDFInteger page : pagesToDelete)
    {
        if (page < 1 || page > pageCount)
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page number '%1'.").arg(page),
                                   options.outputCodec);
            return ErrorInvalidArguments;
        }
    }

    if (pagesToDelete.size() >= size_t(pageCount))
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Cannot delete all pages of the document."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    try
    {
        pdf::PDFDocumentBuilder documentBuilder(&document);
        documentBuilder.flattenPageTree();
        std::vector<pdf::PDFObjectReference> pageReferences = documentBuilder.getPages();

        // Keep every page that is not selected for deletion. The page objects
        // of removed pages become unreferenced and are dropped by the
        // optimizer below (content streams, resources, annotations included).
        std::vector<pdf::PDFObjectReference> keptPages;
        keptPages.reserve(pageReferences.size() - pagesToDelete.size());
        size_t deleteIndex = 0;
        for (size_t i = 0; i < pageReferences.size(); ++i)
        {
            if (deleteIndex < pagesToDelete.size() && pagesToDelete[deleteIndex] == pdf::PDFInteger(i + 1))
            {
                ++deleteIndex; // page removed
            }
            else
            {
                keptPages.push_back(pageReferences[i]);
            }
        }

        documentBuilder.setPages(keptPages);
        pdf::PDFDocument modifiedDocument = documentBuilder.build();

        // Optimize the document - remove unused objects of the deleted pages
        // and shrink the object storage.
        pdf::PDFOptimizer optimizer(pdf::PDFOptimizer::RemoveUnusedObjects | pdf::PDFOptimizer::ShrinkObjectStorage,
                                    nullptr);
        optimizer.setDocument(&modifiedDocument);
        optimizer.optimize();
        modifiedDocument = optimizer.takeOptimizedDocument();

        pdf::PDFDocumentWriter writer(nullptr);
        pdf::PDFOperationResult writeResult = writer.write(outputDocument, &modifiedDocument, false);
        if (!writeResult)
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Failed to write document: %1").arg(writeResult.getErrorMessage()),
                options.outputCodec);
            return ErrorFailedWriteToFile;
        }
    }
    catch (const pdf::PDFException& exception)
    {
        PDFConsole::writeError(exception.getMessage(), options.outputCodec);
        return ErrorUnknown;
    }

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("delete-page", PDFToolTranslationContext::tr("Delete pages"));
    for (const pdf::PDFInteger page : pagesToDelete)
    {
        formatter.writeText("deleted", PDFToolTranslationContext::tr("Deleted page %1.").arg(page));
    }
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolDeletePage::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | DeletePage;
}

} // namespace pdftool
