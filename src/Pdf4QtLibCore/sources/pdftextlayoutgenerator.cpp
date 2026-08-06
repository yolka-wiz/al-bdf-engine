// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
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

#include "pdftextlayoutgenerator.h"
#include "pdfdbgheap.h"
#include "pdfdocument.h"
#include "pdfencoding.h"

namespace pdf
{

PDFTextLayout PDFTextLayoutGenerator::createTextLayout()
{
    m_textLayout.perform();
    m_textLayout.optimize();
    return qMove(m_textLayout);
}

bool PDFTextLayoutGenerator::isContentSuppressedByOC(PDFObjectReference ocgOrOcmd)
{
    if (m_features.testFlag(PDFRenderer::IgnoreOptionalContent))
    {
        return false;
    }

    return PDFPageContentProcessor::isContentSuppressedByOC(ocgOrOcmd);
}

bool PDFTextLayoutGenerator::isContentKindSuppressed(ContentKind kind) const
{
    switch (kind)
    {
    case ContentKind::Shapes:
    case ContentKind::Text:
    case ContentKind::Images:
    case ContentKind::Shading:
        return true;

    case ContentKind::Tiling:
        return false; // Tiling can have text

    case ContentKind::Forms:
        return false; // Forms can have text

    default: {
        Q_ASSERT(false);
        break;
    }
    }

    return false;
}

void PDFTextLayoutGenerator::performOutputCharacter(const PDFTextCharacterInfo& info)
{
    if (!isContentSuppressed() && !info.character.isSpace())
    {
        m_textLayout.addCharacter(info);
    }
}

void PDFTextLayoutGenerator::performMarkedContentBegin(const QByteArray& tag, const PDFObject& properties)
{
    Q_UNUSED(tag);

    ActualTextSpan span;
    span.startIndex = m_textLayout.getCharacterCount();

    // /ActualText is a text string (hex or literal) in the marked-content
    // property dictionary. PDFEncoding::convertTextString decodes the BOM'd
    // UTF-16BE payload the RTL writer emits (and PDFDocEncoding otherwise).
    if (const PDFDictionary* dictionary = getDocument()->getDictionaryFromObject(properties))
    {
        if (dictionary->hasKey("ActualText"))
        {
            const PDFObject& actualTextObject = getDocument()->getObject(dictionary->get("ActualText"));
            if (actualTextObject.isString())
            {
                span.actualText = PDFEncoding::convertTextString(actualTextObject.getString());
            }
        }
    }

    m_actualTextSpans.push_back(std::move(span));
}

void PDFTextLayoutGenerator::performMarkedContentEnd()
{
    if (m_actualTextSpans.empty())
    {
        return;
    }

    ActualTextSpan span = std::move(m_actualTextSpans.back());
    m_actualTextSpans.pop_back();

    if (span.actualText.isEmpty())
    {
        return;
    }

    // The span text is the visual-order text of the glyphs added between the
    // matching BDC and EMC (the RTL writer concatenates each glyph's cluster
    // text in glyph order). Replace the layout range with it: this repairs
    // ligatures the ToUnicode CMap degraded to one UTF-16 unit (lam-alef:
    // 'ل' -> 'لا') and deduplicates mark glyphs that share their base's
    // cluster (decomposed yeh: 'يي' -> 'ي'). PDFTextLayout::replaceCharacters
    // reuses each original glyph slot's geometry, so docstrum line detection
    // and the per-character bounding boxes stay aligned.
    const size_t glyphCount = m_textLayout.getCharacterCount() - span.startIndex;
    if (glyphCount > 0)
    {
        m_textLayout.replaceCharacters(span.startIndex, glyphCount, span.actualText);
    }
}

} // namespace pdf
