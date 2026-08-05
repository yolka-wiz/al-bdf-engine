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

#include "pdftooladdtext.h"

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdffont.h"
#include "pdfmeshqualitysettings.h"
#include "pdfobject.h"
#include "pdfoptionalcontent.h"
#include "pdfpagecontenteditorcontentstreambuilder.h"
#include "pdfpagecontenteditorprocessor.h"
#include "pdfrtltextengine.h"
#include "pdfstreamfilters.h"

#include <QFile>
#include <QFileInfo>

namespace pdftool
{

static PDFToolAddText s_addTextApplication;

QString PDFToolAddText::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "add-text";

    case Name:
        return PDFToolTranslationContext::tr("Add text");

    case Description:
        return PDFToolTranslationContext::tr("Add a text label to a page of the document.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

namespace
{
/// Builds the PDF font dictionary for a standard Helvetica Type1 font and
/// realizes it through the font cache. No embedding is needed for standard
/// fonts; this keeps the LTR path deterministic and dependency-free.
pdf::PDFFontPointer createTextFont(const pdf::PDFDocument& document,
                                   pdf::PDFFontCache& fontCache,
                                   pdf::PDFDictionary& fontDictionary,
                                   const pdf::PDFOptionalContentActivity& activity)
{
    // Build a standard font dictionary: /F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>
    pdf::PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << pdf::PDFObject::createName("Font");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << pdf::PDFObject::createName("Type1");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BaseFont");
    factory << pdf::PDFObject::createName("Helvetica");
    factory.endDictionaryItem();
    factory.endDictionary();
    pdf::PDFObject fontObject = factory.takeObject();

    const QByteArray fontKey = "F1";
    fontDictionary.addEntry(pdf::PDFInplaceOrMemoryString(fontKey), pdf::PDFObject(fontObject));

    // Realize through the cache so the font id matches the resource key.
    pdf::PDFFontPointer font = fontCache.getFont(fontObject, fontKey);
    return font;
}
} // namespace

int PDFToolAddText::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    if (options.addTextOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    if (options.addTextPage.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Page number (--page) must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    if (options.addText.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Text to add (--text) must be specified."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    bool pageOk = false;
    const pdf::PDFInteger pageNumber = options.addTextPage.toInt(&pageOk);
    if (!pageOk || pageNumber < 1 || pageNumber > pdf::PDFInteger(document.getCatalog()->getPageCount()))
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page number '%1'.").arg(options.addTextPage),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    const pdf::PDFInteger pageIndex = pageNumber - 1;

    bool xOk = false, yOk = false;
    const pdf::PDFReal x = options.addTextX.toDouble(&xOk);
    const pdf::PDFReal y = options.addTextY.toDouble(&yOk);
    if (!xOk || !yOk)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid position (--x / --y)."), options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFReal fontSize = 12.0;
    if (!options.addTextFontSize.isEmpty())
    {
        bool sizeOk = false;
        fontSize = options.addTextFontSize.toDouble(&sizeOk);
        if (!sizeOk || fontSize <= 0.0)
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid font size (--size)."), options.outputCodec);
            return ErrorInvalidArguments;
        }
    }

    // Rendering context (same as delete-object).
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
    Q_ASSERT(page);

    // ------------------------------------------------------------------
    // RTL path (--rtl): FriBidi bidi + HarfBuzz shaping + embedded Type0
    // font + ToUnicode + /ActualText. Produces the font dict and the content
    // fragment; the fragment is written as an additional content stream of
    // the page (Contents becomes an array of the original + the new one).
    // ------------------------------------------------------------------
    if (options.addTextRTL)
    {
        if (options.addTextFont.isEmpty())
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("RTL text requires --font <ttf-file>."),
                                   options.outputCodec);
            return ErrorInvalidArguments;
        }
        QFile fontFile(options.addTextFont);
        if (!fontFile.open(QIODevice::ReadOnly))
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Cannot open font file '%1'.").arg(options.addTextFont),
                options.outputCodec);
            return ErrorInvalidArguments;
        }
        const QByteArray fontData = fontFile.readAll();
        const QString language = options.addTextLanguage.isEmpty() ? QStringLiteral("fa") : options.addTextLanguage;

        pdf::PDFRTLTextEngine::Settings rtlSettings;
        rtlSettings.text = options.addText;
        rtlSettings.language = language;
        rtlSettings.fontSize = fontSize;
        rtlSettings.x = x;
        rtlSettings.y = y;
        rtlSettings.fontData = fontData;
        rtlSettings.fontFamily = QFileInfo(options.addTextFont).completeBaseName();

        QByteArray fontKey = "F2";

        // The RTL font must not collide with an existing font resource key on
        // the target page (real-world PDFs use F1/F2/F3/... arbitrarily, and
        // the page Resources may be an INDIRECT reference, e.g. "29 0 R").
        // Resolve the page's Font resources and pick the first free F<N> key.
        // We must resolve through the document object table — page resources
        // are frequently indirect references in real-world PDFs.
        {
            const pdf::PDFObject& pageObject = document.getObjectByReference(page->getPageReference());
            if (const pdf::PDFDictionary* pageDict = pageObject.getDictionary())
            {
                const pdf::PDFObject& resourcesObject = pageDict->get("Resources");
                const pdf::PDFDictionary* resourcesDict = nullptr;
                if (resourcesObject.isDictionary())
                {
                    resourcesDict = resourcesObject.getDictionary();
                }
                else if (resourcesObject.isReference())
                {
                    resourcesDict = document.getObjectByReference(resourcesObject.getReference()).getDictionary();
                }

                if (resourcesDict)
                {
                    const pdf::PDFObject& fontsObject = resourcesDict->get("Font");
                    const pdf::PDFDictionary* fontsDict = nullptr;
                    if (fontsObject.isDictionary())
                    {
                        fontsDict = fontsObject.getDictionary();
                    }
                    else if (fontsObject.isReference())
                    {
                        fontsDict = document.getObjectByReference(fontsObject.getReference()).getDictionary();
                    }

                    if (fontsDict)
                    {
                        for (int n = 2; n < 64; ++n)
                        {
                            const QByteArray candidate = "F" + QByteArray::number(n);
                            bool used = false;
                            for (size_t i = 0; i < fontsDict->getCount(); ++i)
                            {
                                if (fontsDict->getKey(i).getString() == candidate)
                                {
                                    used = true;
                                    break;
                                }
                            }
                            if (!used)
                            {
                                fontKey = candidate;
                                break;
                            }
                        }
                    }
                }
            }
        }
        pdf::PDFRTLTextEngine::Result rtlResult = pdf::PDFRTLTextEngine::create(rtlSettings, fontKey);
        if (!rtlResult.errors.isEmpty())
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("RTL text shaping failed: %1")
                                       .arg(rtlResult.errors.join(QStringLiteral(", "))),
                                   options.outputCodec);
            return ErrorFailedWriteToFile;
        }

        pdf::PDFDocumentModifier modifier(&document);
        pdf::PDFDocumentBuilder* builder = modifier.getBuilder();

        pdf::PDFDictionary fontDictionary = rtlResult.fontDictionary;
        builder->replaceObjectsByReferences(fontDictionary);

        // New content stream for the shaped text.
        pdf::PDFArray filters;
        filters.appendItem(pdf::PDFObject::createName("FlateDecode"));
        const QByteArray compressedData = pdf::PDFFlateDecodeFilter::compress(rtlResult.contentFragment);
        pdf::PDFDictionary contentDictionary;
        contentDictionary.setEntry(pdf::PDFInplaceOrMemoryString("Length"),
                                   pdf::PDFObject::createInteger(compressedData.size()));
        contentDictionary.setEntry(pdf::PDFInplaceOrMemoryString("Filter"),
                                   pdf::PDFObject::createArray(std::make_shared<pdf::PDFArray>(filters)));
        pdf::PDFObject contentObject = pdf::PDFObject::createStream(
            std::make_shared<pdf::PDFStream>(std::move(contentDictionary), QByteArray(compressedData)));

        // Merge the font into the page Resources (KEEPING the existing font
        // entries — the RTL font gets its own key F2 so original text with F1
        // stays intact) and append the new content stream to Contents.
        pdf::PDFObject pageObject = builder->getObjectByReference(page->getPageReference());

        pdf::PDFObjectFactory pageFactory;
        pageFactory.beginDictionary();
        pageFactory.beginDictionaryItem("Resources");
        pageFactory.beginDictionary();

        // Existing font dictionary, if any. The page Resources may be an
        // INDIRECT reference in real-world PDFs (e.g. "29 0 R") — resolve it
        // through the builder's object table so existing fonts are preserved.
        pdf::PDFDictionary mergedFontDict = fontDictionary;
        if (const pdf::PDFDictionary* pageDict = pageObject.getDictionary())
        {
            const pdf::PDFObject& resourcesObject = pageDict->get("Resources");
            const pdf::PDFDictionary* resourcesDict = nullptr;
            if (resourcesObject.isDictionary())
            {
                resourcesDict = resourcesObject.getDictionary();
            }
            else if (resourcesObject.isReference())
            {
                resourcesDict = builder->getObjectByReference(resourcesObject.getReference()).getDictionary();
            }

            if (resourcesDict)
            {
                const pdf::PDFObject& existingFontsObject = resourcesDict->get("Font");
                const pdf::PDFDictionary* existingFontsDict = nullptr;
                if (existingFontsObject.isDictionary())
                {
                    existingFontsDict = existingFontsObject.getDictionary();
                }
                else if (existingFontsObject.isReference())
                {
                    existingFontsDict =
                        builder->getObjectByReference(existingFontsObject.getReference()).getDictionary();
                }

                if (existingFontsDict)
                {
                    for (size_t i = 0; i < existingFontsDict->getCount(); ++i)
                    {
                        const QByteArray key = existingFontsDict->getKey(i).getString();
                        if (!mergedFontDict.hasKey(key))
                        {
                            mergedFontDict.addEntry(pdf::PDFInplaceOrMemoryString(key),
                                                    pdf::PDFObject(existingFontsDict->getValue(i)));
                        }
                    }
                }
            }
        }
        pageFactory.beginDictionaryItem("Font");
        pageFactory << mergedFontDict;
        pageFactory.endDictionaryItem();

        pageFactory.endDictionary();
        pageFactory.endDictionaryItem();

        // Append the RTL content stream: Contents = [existing, new].
        pageFactory.beginDictionaryItem("Contents");
        pageFactory.beginArray();
        const pdf::PDFDictionary* pageDict = pageObject.getDictionary();
        const pdf::PDFObject existingContents = pageDict ? pageDict->get("Contents") : pdf::PDFObject();
        if (existingContents.isReference())
        {
            pageFactory << existingContents;
        }
        else if (existingContents.isStream())
        {
            pageFactory << builder->addObject(existingContents);
        }
        else if (existingContents.isArray())
        {
            const pdf::PDFArray* contentsArray = existingContents.getArray();
            if (contentsArray)
            {
                for (size_t i = 0; i < contentsArray->getCount(); ++i)
                {
                    pageFactory << contentsArray->getItem(i);
                }
            }
        }
        pageFactory << builder->addObject(std::move(contentObject));
        pageFactory.endArray();
        pageFactory.endDictionaryItem();

        pageFactory.endDictionary();

        pageObject = pdf::PDFObjectManipulator::merge(
            pageObject, pageFactory.takeObject(), pdf::PDFObjectManipulator::RemoveNullObjects);
        builder->setObject(page->getPageReference(), std::move(pageObject));

        modifier.markPageContentsChanged();
        if (!modifier.finalize())
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to finalize document modification."),
                                   options.outputCodec);
            return ErrorFailedWriteToFile;
        }

        pdf::PDFDocumentWriter writer(nullptr);
        pdf::PDFOperationResult writeResult =
            writer.write(options.addTextOutputDocument, modifier.getDocument().data(), true);
        if (!writeResult)
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Failed to write document: %1").arg(writeResult.getErrorMessage()),
                options.outputCodec);
            return ErrorFailedWriteToFile;
        }

        PDFOutputFormatter formatter(options.outputStyle);
        formatter.beginDocument("add-text", PDFToolTranslationContext::tr("Add text"));
        formatter.writeText("added",
                            PDFToolTranslationContext::tr("Added RTL text '%1' to page %2 at (%3, %4).")
                                .arg(options.addText)
                                .arg(pageNumber)
                                .arg(x)
                                .arg(y));
        formatter.endDocument();
        PDFConsole::writeText(formatter.getString(), options.outputCodec);

        return ExitSuccess;
    }

    // ------------------------------------------------------------------
    // LTR path (default): standard Helvetica, no embedding.
    // ------------------------------------------------------------------
    // Parse the page content into edited elements, keep them all.
    pdf::PDFPageContentEditorProcessor processor(
        page, &document, &fontCache, cms.data(), &optionalContentActivity, QTransform(), meshQualitySettings);
    processor.processContents();
    pdf::PDFEditedPageContent editedContent = processor.takeEditedPageContent();

    // Create the text element.
    // Build the font (standard Helvetica, no embedding) and register it in the
    // page font dictionary of the edited content.
    pdf::PDFDictionary fontDictionary = editedContent.getFontDictionary();
    pdf::PDFFontPointer font = createTextFont(document, fontCache, fontDictionary, optionalContentActivity);
    editedContent.setFontDictionary(fontDictionary);

    // Build the text element. The content stream builder parses the
    // itemsAsText XML (the format produced by createItemsAsText): <tf> selects
    // the font, <tpos> sets the absolute text position, and character data is
    // encoded via writeTextWithFallback using the font override. For synthetic
    // text we emit that XML directly (glyph paths are only available when
    // parsing existing content).
    pdf::PDFPageContentProcessorState state;
    state.setTextFont(font);
    state.setTextFontSize(fontSize);

    QString itemsAsText = QStringLiteral("<tf font=\"%1\" size=\"%2\"/>%3")
                              .arg(QString::fromLatin1(font->getFontId()))
                              .arg(fontSize)
                              .arg(QString(options.addText).toHtmlEscaped());

    // A single text item carrying the characters. The items are used by
    // recognize-text to extract plain text; the hand-built XML (above) drives
    // rendering, since createItemsAsText only emits characters that carry
    // glyph paths (available only when parsing existing content).
    pdf::PDFEditedPageContentElementText::Item item;
    item.isText = true;
    for (QChar character : options.addText)
    {
        // glyph = nullptr: character-only item (no outline), used for text
        // extraction; rendering is driven by the itemsAsText XML above.
        item.textSequence.items.emplace_back(static_cast<const QPainterPath*>(nullptr), character, 0.0, 0);
    }
    item.state = state;

    std::vector<pdf::PDFEditedPageContentElementText::Item> items = {item};
    pdf::PDFEditedPageContentElementText textElement(
        state, std::move(items), QPainterPath(), QTransform(), itemsAsText);
    textElement.setItemsAsText(itemsAsText);

    // The element transform positions the text at (x, y) in page space; the
    // builder emits it as a cm operator.
    QTransform transform;
    transform.translate(x, y);
    textElement.setTransform(transform);

    editedContent.addContentElement(std::make_unique<pdf::PDFEditedPageContentElementText>(textElement));

    // Serialize everything (original elements + the new text) through the
    // content stream builder.
    pdf::PDFPageContentEditorContentStreamBuilder contentStreamBuilder(&document);
    contentStreamBuilder.setFontDictionary(editedContent.getFontDictionary());
    contentStreamBuilder.setXObjectDictionary(editedContent.getXObjectDictionary());
    contentStreamBuilder.setGraphicStateDictionary(editedContent.getGraphicStateDictionary());

    const size_t elementCount = editedContent.getElementCount();
    for (size_t i = 0; i < elementCount; ++i)
    {
        contentStreamBuilder.writeEditedElement(editedContent.getElement(i));
    }

    if (!contentStreamBuilder.getErrors().isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Content stream serialization failed: %1")
                                   .arg(contentStreamBuilder.getErrors().join(QStringLiteral(", "))),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    // Write-back (same bridge as delete-object).
    pdf::PDFDocumentModifier modifier(&document);
    pdf::PDFDocumentBuilder* builder = modifier.getBuilder();

    pdf::PDFDictionary xobjectDictionary = contentStreamBuilder.getXObjectDictionary();
    pdf::PDFDictionary graphicStateDictionary = contentStreamBuilder.getGraphicStateDictionary();

    builder->replaceObjectsByReferences(fontDictionary);
    builder->replaceObjectsByReferences(xobjectDictionary);
    builder->replaceObjectsByReferences(graphicStateDictionary);

    pdf::PDFArray filters;
    filters.appendItem(pdf::PDFObject::createName("FlateDecode"));
    const QByteArray compressedData = pdf::PDFFlateDecodeFilter::compress(contentStreamBuilder.getOutputContent());

    pdf::PDFDictionary contentDictionary;
    contentDictionary.setEntry(pdf::PDFInplaceOrMemoryString("Length"),
                               pdf::PDFObject::createInteger(compressedData.size()));
    contentDictionary.setEntry(pdf::PDFInplaceOrMemoryString("Filter"),
                               pdf::PDFObject::createArray(std::make_shared<pdf::PDFArray>(filters)));
    pdf::PDFObject contentObject = pdf::PDFObject::createStream(
        std::make_shared<pdf::PDFStream>(std::move(contentDictionary), QByteArray(compressedData)));

    pdf::PDFObject pageObject = builder->getObjectByReference(page->getPageReference());

    pdf::PDFObjectFactory pageFactory;
    pageFactory.beginDictionary();
    pageFactory.beginDictionaryItem("Resources");
    pageFactory.beginDictionary();

    if (!fontDictionary.isEmpty())
    {
        pageFactory.beginDictionaryItem("Font");
        pageFactory << fontDictionary;
        pageFactory.endDictionaryItem();
    }
    if (!xobjectDictionary.isEmpty())
    {
        pageFactory.beginDictionaryItem("XObject");
        pageFactory << xobjectDictionary;
        pageFactory.endDictionaryItem();
    }
    if (!graphicStateDictionary.isEmpty())
    {
        pageFactory.beginDictionaryItem("ExtGState");
        pageFactory << graphicStateDictionary;
        pageFactory.endDictionaryItem();
    }

    pageFactory.endDictionary();
    pageFactory.endDictionaryItem();

    pageFactory.beginDictionaryItem("Contents");
    pageFactory << builder->addObject(std::move(contentObject));
    pageFactory.endDictionaryItem();

    pageFactory.endDictionary();

    pageObject = pdf::PDFObjectManipulator::merge(
        pageObject, pageFactory.takeObject(), pdf::PDFObjectManipulator::RemoveNullObjects);
    builder->setObject(page->getPageReference(), std::move(pageObject));

    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to finalize document modification."),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    pdf::PDFDocumentWriter writer(nullptr);
    pdf::PDFOperationResult writeResult =
        writer.write(options.addTextOutputDocument, modifier.getDocument().data(), true);
    if (!writeResult)
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Failed to write document: %1").arg(writeResult.getErrorMessage()),
            options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("add-text", PDFToolTranslationContext::tr("Add text"));
    formatter.writeText("added",
                        PDFToolTranslationContext::tr("Added text '%1' to page %2 at (%3, %4).")
                            .arg(options.addText)
                            .arg(pageNumber)
                            .arg(x)
                            .arg(y));
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolAddText::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | AddText;
}

} // namespace pdftool
