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

#include "pdftooldeleteobject.h"

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocument.h"
#include "pdffont.h"
#include "pdfmeshqualitysettings.h"
#include "pdfobject.h"
#include "pdfoptionalcontent.h"
#include "pdfpagecontenteditorcontentstreambuilder.h"
#include "pdfpagecontenteditorprocessor.h"
#include "pdfpagecontentrewriter.h"

namespace pdftool
{

static PDFToolDeleteObject s_deleteObjectApplication;

QString PDFToolDeleteObject::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "delete-object";

    case Name:
        return PDFToolTranslationContext::tr("Delete object");

    case Description:
        return PDFToolTranslationContext::tr("Delete a whole content object (text run, image, path) from a page.");

    default:
        break;
    }

    return QString();
}

namespace
{
/// Lists one content object of a page as a table row.
void listObject(PDFOutputFormatter& formatter, size_t index, const pdf::PDFEditedPageContentElement* element)
{
    QString type = QStringLiteral("unknown");
    QString text;

    if (element->asText())
    {
        type = QStringLiteral("text");
        text = element->asText()->getItemsAsText();
    }
    else if (element->asImage())
    {
        type = QStringLiteral("image");
    }
    else if (element->asPath())
    {
        type = QStringLiteral("path");
    }

    formatter.beginTableRow(QStringLiteral("object-%1").arg(index), int(index));
    formatter.writeTableColumn("type", type);
    formatter.writeTableColumn("bbox",
                               QStringLiteral("%1 %2 %3 %4")
                                   .arg(element->getBoundingBox().x())
                                   .arg(element->getBoundingBox().y())
                                   .arg(element->getBoundingBox().width())
                                   .arg(element->getBoundingBox().height()));
    formatter.writeTableColumn("text", text);
    formatter.endTableRow();
}
} // namespace

int PDFToolDeleteObject::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    // The second positional argument is the output document.
    if (options.deleteObjectOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const QString outputDocument = options.deleteObjectOutputDocument;

    if (options.deleteObjectPage.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Page number (--page) must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    bool pageOk = false;
    const pdf::PDFInteger pageNumber = options.deleteObjectPage.toInt(&pageOk);
    if (!pageOk || pageNumber < 1 || pageNumber > pdf::PDFInteger(document.getCatalog()->getPageCount()))
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page number '%1'.").arg(options.deleteObjectPage),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    const pdf::PDFInteger pageIndex = pageNumber - 1;

    // Rendering context: optional content activity, CMS, font cache.
    pdf::PDFOptionalContentActivity optionalContentActivity(&document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(&document);
    cmsManager.setSettings(options.cmsSettings);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();

    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    fontCache.setDocument(pdf::PDFModifiedDocument(&document, &optionalContentActivity));
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFMeshQualitySettings meshQualitySettings;

    const pdf::PDFPage* page = document.getCatalog()->getPage(pageIndex);
    if (!page)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Page %1 does not exist.").arg(pageNumber),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    // Parse the page content into the edited element list. The element order
    // is the content stream order = the index space of recognize-text.
    pdf::PDFPageContentEditorProcessor processor(
        page, &document, &fontCache, cms.data(), &optionalContentActivity, QTransform(), meshQualitySettings);
    processor.processContents();

    const pdf::PDFEditedPageContent& editedContent = processor.getEditedPageContent();
    const size_t elementCount = editedContent.getElementCount();

    // List mode: print all objects and exit without modifying anything.
    if (options.deleteObjectList)
    {
        PDFOutputFormatter formatter(options.outputStyle);
        formatter.beginDocument("delete-object", PDFToolTranslationContext::tr("Delete object"));
        formatter.beginTable("objects", PDFToolTranslationContext::tr("Page objects"));
        formatter.beginTableHeaderRow("header");
        formatter.writeTableHeaderColumn("index", PDFToolTranslationContext::tr("Index"));
        formatter.writeTableHeaderColumn("type", PDFToolTranslationContext::tr("Type"));
        formatter.writeTableHeaderColumn("bbox", PDFToolTranslationContext::tr("Bounding box"));
        formatter.writeTableHeaderColumn("text", PDFToolTranslationContext::tr("Text"));
        formatter.endTableHeaderRow();

        for (size_t i = 0; i < elementCount; ++i)
        {
            listObject(formatter, i, editedContent.getElement(i));
        }

        formatter.endTable();
        formatter.endDocument();
        PDFConsole::writeText(formatter.getString(), options.outputCodec);
        return ExitSuccess;
    }

    if (options.deleteObjectIndex.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Object index (--index) must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    bool indexOk = false;
    const size_t deleteIndex = size_t(options.deleteObjectIndex.toUInt(&indexOk));
    if (!indexOk || deleteIndex >= elementCount)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid object index '%1' (page has %2 objects).")
                                   .arg(options.deleteObjectIndex)
                                   .arg(elementCount),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    // Serialize all elements except the deleted one.
    pdf::PDFPageContentEditorContentStreamBuilder contentStreamBuilder(&document);
    contentStreamBuilder.setFontDictionary(editedContent.getFontDictionary());
    contentStreamBuilder.setXObjectDictionary(editedContent.getXObjectDictionary());
    contentStreamBuilder.setGraphicStateDictionary(editedContent.getGraphicStateDictionary());

    for (size_t i = 0; i < elementCount; ++i)
    {
        if (i == deleteIndex)
        {
            continue;
        }
        contentStreamBuilder.writeEditedElement(editedContent.getElement(i));
    }

    if (!contentStreamBuilder.getErrors().isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Content stream serialization failed: %1")
                                   .arg(contentStreamBuilder.getErrors().join(QStringLiteral(", "))),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    // Deletion safety: the deleted element is simply not re-serialized. Its
    // XObject resource may remain in the page XObject dictionary; that is
    // safe (unused resource entries are ignored by renderers) and avoids
    // breaking other pages that may share the same XObject. Resources are
    // compacted by the optimize pass if desired.
    pdf::PDFPageContentRewriter::Settings rewriteSettings;
    rewriteSettings.pageReference = page->getPageReference();
    rewriteSettings.fontDictionary = contentStreamBuilder.getFontDictionary();
    rewriteSettings.xobjectDictionary = contentStreamBuilder.getXObjectDictionary();
    rewriteSettings.graphicStateDictionary = contentStreamBuilder.getGraphicStateDictionary();
    rewriteSettings.contentBytes = contentStreamBuilder.getOutputContent();
    rewriteSettings.mode = pdf::PDFPageContentRewriter::ContentsMode::Replace;
    rewriteSettings.outputPath = outputDocument;

    pdf::PDFPageContentRewriter::Result rewriteResult = pdf::PDFPageContentRewriter::rewrite(document, rewriteSettings);
    if (!rewriteResult.isSuccess())
    {
        if (rewriteResult.failure == pdf::PDFPageContentRewriter::Failure::Finalize)
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to finalize document modification."),
                                   options.outputCodec);
        }
        else
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Failed to write document: %1").arg(rewriteResult.errorMessage),
                options.outputCodec);
        }
        return ErrorFailedWriteToFile;
    }

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("delete-object", PDFToolTranslationContext::tr("Delete object"));
    formatter.writeText(
        "deleted", PDFToolTranslationContext::tr("Deleted object %1 on page %2.").arg(deleteIndex).arg(pageNumber));
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolDeleteObject::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | DeleteObject;
}

} // namespace pdftool
