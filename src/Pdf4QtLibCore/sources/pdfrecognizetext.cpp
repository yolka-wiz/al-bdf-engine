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

#include "pdfrecognizetext.h"

#include "pdfdocument.h"
#include "pdffont.h"
#include "pdfcms.h"
#include "pdfoptionalcontent.h"
#include "pdfmeshqualitysettings.h"
#include "pdfpagecontenteditorprocessor.h"
#include "pdftextlayoutgenerator.h"
#include "pdfrenderer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace pdf
{

namespace
{
/// Rounds a value to two decimal places, to keep the JSON output tidy while
/// staying accurate enough for PDF points.
double roundToTwoDecimals(double value)
{
    return std::round(value * 100.0) / 100.0;
}

QJsonObject boxToJson(const QRectF& box)
{
    QJsonObject result;
    result.insert(QStringLiteral("x"), roundToTwoDecimals(box.x()));
    result.insert(QStringLiteral("y"), roundToTwoDecimals(box.y()));
    result.insert(QStringLiteral("w"), roundToTwoDecimals(box.width()));
    result.insert(QStringLiteral("h"), roundToTwoDecimals(box.height()));
    return result;
}

QString getFontName(const PDFPageContentProcessorState& state)
{
    if (const PDFFontPointer& font = state.getTextFont())
    {
        if (const FontDescriptor* descriptor = font->getFontDescriptor())
        {
            if (!descriptor->fontName.isEmpty())
            {
                return QString::fromLatin1(descriptor->fontName);
            }
        }

        const QByteArray fontId = font->getFontId();
        if (!fontId.isEmpty())
        {
            return QString::fromLatin1(fontId);
        }
    }

    return QString();
}
}   // namespace

PDFRecognizeText::PDFRecognizeText(const PDFDocument* document,
                                   const PDFFontCache* fontCache,
                                   const PDFCMS* cms,
                                   const PDFOptionalContentActivity* optionalContentActivity,
                                   const PDFMeshQualitySettings* meshQualitySettings) :
    m_document(document),
    m_fontCache(fontCache),
    m_cms(cms),
    m_optionalContentActivity(optionalContentActivity),
    m_meshQualitySettings(meshQualitySettings)
{
}

std::vector<PDFRecognizeText::ObjectInfo> PDFRecognizeText::recognize(PDFInteger pageIndex) const
{
    std::vector<ObjectInfo> result;

    if (!m_document)
    {
        return result;
    }

    const PDFPage* page = m_document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return result;
    }

    // Parse the page content into the edited page content element list. The
    // element order (and thus the emitted indices) is the content stream
    // order, which is the index space shared with the delete-object command.
    PDFPageContentEditorProcessor processor(page,
                                            m_document,
                                            m_fontCache,
                                            m_cms,
                                            m_optionalContentActivity,
                                            QTransform(),
                                            *m_meshQualitySettings);
    processor.processContents();

    const PDFEditedPageContent& content = processor.getEditedPageContent();

    // Per-character bounding boxes of the whole page. The layout generator
    // works in device space; with an identity page-to-device matrix, that is
    // the page user space (PDF points), the same space as the element
    // bounding boxes. Space characters are not emitted by the generator.
    std::vector<QRectF> pageCharBoxes;
    {
        PDFTextLayoutGenerator layoutGenerator(PDFRenderer::IgnoreOptionalContent,
                                               page,
                                               m_document,
                                               m_fontCache,
                                               m_cms,
                                               m_optionalContentActivity,
                                               QTransform(),
                                               *m_meshQualitySettings);
        // Must process the page contents first: performOutputCharacter() fills
        // the layout storage; createTextLayout() only lays it out.
        layoutGenerator.processContents();
        const PDFTextLayout layout = layoutGenerator.createTextLayout();
        const PDFTextBlocks& blocks = layout.getTextBlocks();
        for (const PDFTextBlock& block : blocks)
        {
            for (const PDFTextLine& line : block.getLines())
            {
                for (const TextCharacter& character : line.getCharacters())
                {
                    pageCharBoxes.push_back(character.boundingBox.boundingRect());
                }
            }
        }
    }

    const int pageNumber = static_cast<int>(pageIndex) + 1;

    for (size_t index = 0; index < content.getElementCount(); ++index)
    {
        const PDFEditedPageContentElement* element = content.getElement(index);

        ObjectInfo info;
        info.page = pageNumber;
        info.index = static_cast<int>(index);
        info.bbox = element->getBoundingBox();

        switch (element->getType())
        {
            case PDFEditedPageContentElement::Type::Text:
            {
                info.type = QStringLiteral("text");

                const PDFEditedPageContentElementText* textElement = element->asText();
                info.text = textElement->getItemsAsText();

                // Font and font size are captured in the per-item graphic states.
                for (const PDFEditedPageContentElementText::Item& item : textElement->getItems())
                {
                    if (item.state.getTextFont())
                    {
                        info.font = getFontName(item.state);
                        info.fontSize = item.state.getTextFontSize();
                        break;
                    }
                }

                // Assign page characters to this element by geometric
                // containment in the element bounding box.
                if (!info.bbox.isEmpty())
                {
                    const QRectF expandedBBox = info.bbox.adjusted(-1.0, -1.0, 1.0, 1.0);
                    for (const QRectF& charBox : pageCharBoxes)
                    {
                        if (expandedBBox.contains(charBox.center()))
                        {
                            info.charBoxes.push_back(charBox);
                        }
                    }
                }
                break;
            }

            case PDFEditedPageContentElement::Type::Image:
                info.type = QStringLiteral("image");
                break;

            case PDFEditedPageContentElement::Type::Path:
                info.type = QStringLiteral("path");
                break;

            default:
                Q_ASSERT(false);
                break;
        }

        result.push_back(std::move(info));
    }

    return result;
}

QString PDFRecognizeText::toJson(const std::vector<ObjectInfo>& objects)
{
    // Group the objects by page, preserving page order.
    QJsonArray pagesArray;
    QJsonArray currentObjects;
    int currentPage = 0;

    auto flushPage = [&pagesArray, &currentObjects, &currentPage]()
    {
        if (!currentObjects.isEmpty())
        {
            QJsonObject pageObject;
            pageObject.insert(QStringLiteral("page"), currentPage);
            pageObject.insert(QStringLiteral("objects"), currentObjects);
            pagesArray.append(pageObject);
            currentObjects = QJsonArray();
        }
    };

    for (const ObjectInfo& object : objects)
    {
        if (object.page != currentPage)
        {
            flushPage();
            currentPage = object.page;
        }

        QJsonObject objectJson;
        objectJson.insert(QStringLiteral("page"), object.page);
        objectJson.insert(QStringLiteral("index"), object.index);
        objectJson.insert(QStringLiteral("type"), object.type);
        objectJson.insert(QStringLiteral("bbox"), boxToJson(object.bbox));

        if (object.type == QStringLiteral("text"))
        {
            objectJson.insert(QStringLiteral("text"), object.text);
            objectJson.insert(QStringLiteral("font"), object.font);
            objectJson.insert(QStringLiteral("size"), roundToTwoDecimals(object.fontSize));

            QJsonArray charBoxesArray;
            for (const QRectF& charBox : object.charBoxes)
            {
                charBoxesArray.append(boxToJson(charBox));
            }
            objectJson.insert(QStringLiteral("charBoxes"), charBoxesArray);
        }
        else
        {
            objectJson.insert(QStringLiteral("charBoxes"), QJsonArray());
        }

        currentObjects.append(objectJson);
    }

    flushPage();

    QJsonObject root;
    root.insert(QStringLiteral("pages"), pagesArray);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

}   // namespace pdf
