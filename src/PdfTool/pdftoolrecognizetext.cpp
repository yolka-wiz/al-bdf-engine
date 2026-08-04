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

#include "pdftoolrecognizetext.h"

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdffont.h"
#include "pdfmeshqualitysettings.h"
#include "pdfoptionalcontent.h"
#include "pdfrecognizetext.h"

namespace pdftool
{

static PDFToolRecognizeText s_recognizeTextApplication;

QString PDFToolRecognizeText::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "recognize-text";

    case Name:
        return PDFToolTranslationContext::tr("Recognize text");

    case Description:
        return PDFToolTranslationContext::tr(
            "Recognize page content objects (text, image, path) and print them with bounding boxes.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

int PDFToolRecognizeText::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    // Prepare the rendering context: optional content activity, CMS and font cache.
    pdf::PDFOptionalContentActivity optionalContentActivity(&document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(&document);
    cmsManager.setSettings(options.cmsSettings);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();

    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    fontCache.setDocument(pdf::PDFModifiedDocument(&document, &optionalContentActivity));
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFMeshQualitySettings meshQualitySettings;

    pdf::PDFRecognizeText recognizer(&document, &fontCache, cms.data(), &optionalContentActivity, &meshQualitySettings);

    QString errorMessage;
    std::vector<pdf::PDFInteger> pages =
        options.getPageRange(document.getCatalog()->getPageCount(), errorMessage, false);
    if (!errorMessage.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page range: %1").arg(errorMessage),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    std::vector<pdf::PDFRecognizeText::ObjectInfo> allObjects;
    for (pdf::PDFInteger page : pages)
    {
        // getPageRange returns 1-based page numbers (zeroBased = false).
        std::vector<pdf::PDFRecognizeText::ObjectInfo> objects = recognizer.recognize(page - 1);
        allObjects.insert(allObjects.end(), objects.begin(), objects.end());
    }

    if (options.outputStyle == PDFOutputFormatter::Style::Xml)
    {
        // Structured XML via the standard formatter.
        PDFOutputFormatter formatter(options.outputStyle);
        formatter.beginDocument("recognize-text", PDFToolTranslationContext::tr("Recognize text"));
        for (const pdf::PDFRecognizeText::ObjectInfo& object : allObjects)
        {
            formatter.beginElement(PDFOutputFormatter::Element::Text, QStringLiteral("object-%1").arg(object.index));
            formatter.writeText("page", QString::number(object.page));
            formatter.writeText("index", QString::number(object.index));
            formatter.writeText("type", object.type);
            formatter.writeText("bbox",
                                QStringLiteral("%1 %2 %3 %4")
                                    .arg(object.bbox.x())
                                    .arg(object.bbox.y())
                                    .arg(object.bbox.width())
                                    .arg(object.bbox.height()));
            if (!object.text.isEmpty())
            {
                formatter.writeText("text", object.text);
            }
            formatter.endElement();
        }
        formatter.endDocument();
        PDFConsole::writeText(formatter.getString(), options.outputCodec);
    }
    else
    {
        // Text and HTML styles: emit the raw JSON document (machine-readable,
        // parseable as-is) without the formatter wrapper.
        PDFConsole::writeText(pdf::PDFRecognizeText::toJson(allObjects), options.outputCodec);
    }

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolRecognizeText::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | PageSelector;
}

} // namespace pdftool
