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

#include "pdftoolrotate.h"

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfexception.h"

#include <algorithm>

namespace pdftool
{

static PDFToolRotate s_rotateApplication;

QString PDFToolRotate::getStandardString(PDFToolAbstractApplication::StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "rotate";

    case Name:
        return PDFToolTranslationContext::tr("Rotate pages");

    case Description:
        return PDFToolTranslationContext::tr("Rotate pages of a document by 90, 180 or 270 degrees clockwise.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

namespace
{
/// Converts a clockwise rotation angle in degrees to a PageRotation value.
/// Returns PageRotation::None for angles other than 90, 180 and 270.
pdf::PageRotation angleToPageRotation(int angle)
{
    switch (angle)
    {
    case 90:
        return pdf::PageRotation::Rotate90;
    case 180:
        return pdf::PageRotation::Rotate180;
    case 270:
        return pdf::PageRotation::Rotate270;
    default:
        return pdf::PageRotation::None;
    }
}
} // namespace

int PDFToolRotate::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    // The second positional argument is the output document.
    if (options.rotateOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const QString outputDocument = options.rotateOutputDocument;

    if (options.rotatePages.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Page number (--page) must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    bool angleOk = false;
    const int angle = options.rotateAngle.toInt(&angleOk);
    const pdf::PageRotation angleRotation = angleToPageRotation(angle);
    if (!angleOk || angleRotation == pdf::PageRotation::None)
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Invalid rotation angle '%1'. Valid values are 90, 180 and 270.")
                .arg(options.rotateAngle),
            options.outputCodec);
        return ErrorInvalidArguments;
    }

    const pdf::PDFInteger pageCount = document.getCatalog()->getPageCount();

    std::vector<pdf::PDFInteger> pageIndices; // 0-based, deduplicated
    for (const QString& pageText : options.rotatePages)
    {
        bool pageOk = false;
        const pdf::PDFInteger pageNumber = pageText.toInt(&pageOk);
        if (!pageOk || pageNumber < 1 || pageNumber > pageCount)
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page number '%1'.").arg(pageText),
                                   options.outputCodec);
            return ErrorInvalidArguments;
        }

        const pdf::PDFInteger pageIndex = pageNumber - 1;
        if (std::find(pageIndices.cbegin(), pageIndices.cend(), pageIndex) == pageIndices.cend())
        {
            pageIndices.push_back(pageIndex);
        }
    }

    try
    {
        pdf::PDFDocumentBuilder documentBuilder(&document);
        documentBuilder.flattenPageTree();
        std::vector<pdf::PDFObjectReference> pageReferences = documentBuilder.getPages();

        // Rotation is relative and clockwise: combine with the current page
        // rotation. Only the /Rotate page attribute changes, so page content,
        // resources and annotations are preserved.
        for (const pdf::PDFInteger pageIndex : pageIndices)
        {
            const pdf::PDFPage* page = document.getCatalog()->getPage(size_t(pageIndex));
            const pdf::PageRotation newRotation = pdf::getPageRotationCombined(page->getPageRotation(), angleRotation);
            documentBuilder.setPageRotation(pageReferences[size_t(pageIndex)], newRotation);
        }

        pdf::PDFDocument modifiedDocument = documentBuilder.build();

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
    formatter.beginDocument("rotate", PDFToolTranslationContext::tr("Rotate pages"));
    for (const pdf::PDFInteger pageIndex : pageIndices)
    {
        formatter.writeText(
            "rotated",
            PDFToolTranslationContext::tr("Rotated page %1 by %2 degrees clockwise.").arg(pageIndex + 1).arg(angle));
    }
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolRotate::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | Rotate;
}

} // namespace pdftool
