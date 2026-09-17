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

#include "pdfpagecontentrewriter.h"

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfstreamfilters.h"

#include <memory>

namespace pdf
{

void PDFPageContentRewriter::resolveIndirectResources(PDFDocumentBuilder* builder, PDFObject& pageObject)
{
    const PDFDictionary* pageDictionary = pageObject.getDictionary();
    if (!pageDictionary)
    {
        return;
    }

    const PDFObject& resourcesObject = pageDictionary->get("Resources");
    if (!resourcesObject.isReference())
    {
        return;
    }

    const PDFObject& resolvedResources = builder->getObjectByReference(resourcesObject.getReference());
    if (!resolvedResources.isDictionary())
    {
        return;
    }

    // Inline the referenced dictionary, then merge() below sees a direct
    // dictionary and keeps every existing resource instead of replacing the
    // whole /Resources entry.
    PDFDictionary pageDictionaryCopy = *pageDictionary;
    pageDictionaryCopy.setEntry(PDFInplaceOrMemoryString("Resources"), PDFObject(resolvedResources));
    pageObject = PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(pageDictionaryCopy)));
}

void PDFPageContentRewriter::mergeExistingFonts(const PDFDocumentBuilder* builder,
                                                const PDFObject& pageObject,
                                                PDFDictionary& fontDictionary)
{
    const PDFDictionary* pageDictionary = pageObject.getDictionary();
    if (!pageDictionary)
    {
        return;
    }

    const PDFObject& resourcesObject = pageDictionary->get("Resources");
    const PDFDictionary* resourcesDictionary = nullptr;
    if (resourcesObject.isDictionary())
    {
        resourcesDictionary = resourcesObject.getDictionary();
    }
    else if (resourcesObject.isReference())
    {
        resourcesDictionary = builder->getObjectByReference(resourcesObject.getReference()).getDictionary();
    }

    if (!resourcesDictionary)
    {
        return;
    }

    const PDFObject& existingFontsObject = resourcesDictionary->get("Font");
    const PDFDictionary* existingFontsDictionary = nullptr;
    if (existingFontsObject.isDictionary())
    {
        existingFontsDictionary = existingFontsObject.getDictionary();
    }
    else if (existingFontsObject.isReference())
    {
        existingFontsDictionary = builder->getObjectByReference(existingFontsObject.getReference()).getDictionary();
    }

    if (!existingFontsDictionary)
    {
        return;
    }

    for (size_t i = 0; i < existingFontsDictionary->getCount(); ++i)
    {
        const QByteArray key = existingFontsDictionary->getKey(i).getString();
        if (!fontDictionary.hasKey(key))
        {
            fontDictionary.addEntry(PDFInplaceOrMemoryString(key), PDFObject(existingFontsDictionary->getValue(i)));
        }
    }
}

PDFObject PDFPageContentRewriter::buildContentStream(const QByteArray& uncompressedContent)
{
    PDFArray filters;
    filters.appendItem(PDFObject::createName("FlateDecode"));
    const QByteArray compressedData = PDFFlateDecodeFilter::compress(uncompressedContent);

    PDFDictionary contentDictionary;
    contentDictionary.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(compressedData.size()));
    contentDictionary.setEntry(PDFInplaceOrMemoryString("Filter"),
                               PDFObject::createArray(std::make_shared<PDFArray>(filters)));

    return PDFObject::createStream(
        std::make_shared<PDFStream>(std::move(contentDictionary), QByteArray(compressedData)));
}

void PDFPageContentRewriter::applyReplace(PDFDocumentBuilder* builder, const Settings& settings)
{
    PDFDictionary fontDictionary = settings.fontDictionary;
    PDFDictionary xobjectDictionary = settings.xobjectDictionary;
    PDFDictionary graphicStateDictionary = settings.graphicStateDictionary;

    builder->replaceObjectsByReferences(fontDictionary);
    builder->replaceObjectsByReferences(xobjectDictionary);
    builder->replaceObjectsByReferences(graphicStateDictionary);

    PDFObject pageObject = builder->getObjectByReference(settings.pageReference);
    resolveIndirectResources(builder, pageObject);

    PDFObjectFactory pageFactory;
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
    pageFactory << builder->addObject(buildContentStream(settings.contentBytes));
    pageFactory.endDictionaryItem();

    pageFactory.endDictionary();

    pageObject =
        PDFObjectManipulator::merge(pageObject, pageFactory.takeObject(), PDFObjectManipulator::RemoveNullObjects);
    builder->setObject(settings.pageReference, std::move(pageObject));
}

void PDFPageContentRewriter::applyAppend(PDFDocumentBuilder* builder, const Settings& settings)
{
    PDFDictionary fontDictionary = settings.fontDictionary;
    builder->replaceObjectsByReferences(fontDictionary);

    PDFObject pageObject = builder->getObjectByReference(settings.pageReference);
    resolveIndirectResources(builder, pageObject);

    PDFDictionary mergedFontDictionary = fontDictionary;
    mergeExistingFonts(builder, pageObject, mergedFontDictionary);

    PDFObjectFactory pageFactory;
    pageFactory.beginDictionary();
    pageFactory.beginDictionaryItem("Resources");
    pageFactory.beginDictionary();
    pageFactory.beginDictionaryItem("Font");
    pageFactory << mergedFontDictionary;
    pageFactory.endDictionaryItem();
    pageFactory.endDictionary();
    pageFactory.endDictionaryItem();

    pageFactory.beginDictionaryItem("Contents");
    pageFactory.beginArray();
    const PDFDictionary* pageDictionary = pageObject.getDictionary();
    const PDFObject existingContents = pageDictionary ? pageDictionary->get("Contents") : PDFObject();
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
        const PDFArray* contentsArray = existingContents.getArray();
        if (contentsArray)
        {
            for (size_t i = 0; i < contentsArray->getCount(); ++i)
            {
                pageFactory << contentsArray->getItem(i);
            }
        }
    }
    pageFactory << builder->addObject(buildContentStream(settings.contentBytes));
    pageFactory.endArray();
    pageFactory.endDictionaryItem();

    pageFactory.endDictionary();

    pageObject =
        PDFObjectManipulator::merge(pageObject, pageFactory.takeObject(), PDFObjectManipulator::RemoveNullObjects);
    builder->setObject(settings.pageReference, std::move(pageObject));
}

PDFPageContentRewriter::Result PDFPageContentRewriter::rewrite(PDFDocument& document, const Settings& settings)
{
    PDFDocumentModifier modifier(&document);
    PDFDocumentBuilder* builder = modifier.getBuilder();

    if (settings.mode == ContentsMode::Append)
    {
        applyAppend(builder, settings);
    }
    else
    {
        applyReplace(builder, settings);
    }

    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        Result result;
        result.failure = Failure::Finalize;
        return result;
    }

    PDFDocumentWriter writer(nullptr);
    PDFOperationResult writeResult = writer.write(settings.outputPath, modifier.getDocument().data(), true);
    if (!writeResult)
    {
        Result result;
        result.failure = Failure::Write;
        result.errorMessage = writeResult.getErrorMessage();
        return result;
    }

    return Result();
}

} // namespace pdf
