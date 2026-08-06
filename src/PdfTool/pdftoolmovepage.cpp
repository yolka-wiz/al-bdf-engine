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

#include "pdftoolmovepage.h"

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfexception.h"
#include "pdfoptimizer.h"

namespace pdftool
{

static PDFToolMovePage s_movePageApplication;

QString PDFToolMovePage::getStandardString(PDFToolAbstractApplication::StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "move-page";

    case Name:
        return PDFToolTranslationContext::tr("Move page");

    case Description:
        return PDFToolTranslationContext::tr("Move a page of a document to a new position.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

int PDFToolMovePage::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    // The second positional argument is the output document.
    if (options.movePageOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const QString outputDocument = options.movePageOutputDocument;

    if (options.movePageFrom.isEmpty() || options.movePageTo.isEmpty())
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Source page (--from) and target position (--to) must be specified."),
            options.outputCodec);
        return ErrorInvalidArguments;
    }

    const pdf::PDFInteger pageCount = document.getCatalog()->getPageCount();

    bool fromOk = false;
    bool toOk = false;
    const pdf::PDFInteger fromPage = options.movePageFrom.toInt(&fromOk);
    const pdf::PDFInteger toPage = options.movePageTo.toInt(&toOk);

    if (!fromOk || fromPage < 1 || fromPage > pageCount)
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Invalid source page number '%1'.").arg(options.movePageFrom),
            options.outputCodec);
        return ErrorInvalidArguments;
    }

    if (!toOk || toPage < 1 || toPage > pageCount)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid target position '%1'.").arg(options.movePageTo),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    try
    {
        pdf::PDFDocumentBuilder documentBuilder(&document);
        documentBuilder.flattenPageTree();
        std::vector<pdf::PDFObjectReference> pageReferences = documentBuilder.getPages();

        // Relocate the page: remove it from its current slot and insert it at
        // the target position. The page object is moved as a whole, so its
        // content, resources and annotations stay intact. Moving a page to
        // its own position is a no-op (still valid).
        const size_t fromIndex = size_t(fromPage - 1);
        const size_t toIndex = size_t(toPage - 1);
        pdf::PDFObjectReference movedPage = pageReferences[fromIndex];
        pageReferences.erase(pageReferences.cbegin() + fromIndex);
        pageReferences.insert(pageReferences.cbegin() + toIndex, movedPage);

        documentBuilder.setPages(pageReferences);
        pdf::PDFDocument modifiedDocument = documentBuilder.build();

        // Optimize the document - drop unused objects (old page-tree nodes)
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
    formatter.beginDocument("move-page", PDFToolTranslationContext::tr("Move page"));
    formatter.writeText("moved",
                        PDFToolTranslationContext::tr("Moved page %1 to position %2.").arg(fromPage).arg(toPage));
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolMovePage::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | MovePage;
}

} // namespace pdftool
