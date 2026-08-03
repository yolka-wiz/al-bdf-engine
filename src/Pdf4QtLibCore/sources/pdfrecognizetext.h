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

#ifndef PDFRECOGNIZETEXT_H
#define PDFRECOGNIZETEXT_H

#include "pdfglobal.h"

#include <QString>
#include <QRectF>

#include <vector>

namespace pdf
{

class PDFDocument;
class PDFFontCache;
class PDFCMS;
class PDFOptionalContentActivity;
class PDFMeshQualitySettings;

/// Recognizes page content objects (text, image, path) of a PDF document
/// and serializes them as machine-readable JSON. It is the engine behind
/// the PdfTool 'recognize-text' CLI command.
///
/// Index contract: the \p index emitted for each object is the 0-based index
/// of the element in the PDFEditedPageContent element list produced by
/// PDFPageContentEditorProcessor for that page (in content stream order).
/// The same index space is consumed by the PdfTool 'delete-object' command,
/// so indices are stable between recognize and delete on the same document.
class PDF4QTLIBCORESHARED_EXPORT PDFRecognizeText
{
public:
    /// Information about a single page content object.
    struct ObjectInfo
    {
        int page = 0;                  ///< Page number, 1-based
        int index = 0;                 ///< Object index on the page, 0-based (delete-object contract)
        QString type;                  ///< "text", "image" or "path"
        QRectF bbox;                   ///< Bounding box in PDF points (page user space)
        QString text;                  ///< Extracted text (text objects only)
        QString font;                  ///< Font name (text objects only)
        PDFReal fontSize = 0.0;        ///< Font size (text objects only)
        std::vector<QRectF> charBoxes; ///< Per-character bounding boxes in PDF points (text objects only)
    };

    /// \param document Document to analyze
    /// \param fontCache Font cache (must be initialized for the document)
    /// \param cms Color management system
    /// \param optionalContentActivity Optional content activity
    /// \param meshQualitySettings Mesh quality settings (used by the content processor)
    explicit PDFRecognizeText(const PDFDocument* document,
                              const PDFFontCache* fontCache,
                              const PDFCMS* cms,
                              const PDFOptionalContentActivity* optionalContentActivity,
                              const PDFMeshQualitySettings* meshQualitySettings);

    /// Recognizes all content objects on a single page.
    /// \param pageIndex Page index, 0-based
    std::vector<ObjectInfo> recognize(PDFInteger pageIndex) const;

    /// Serializes recognized objects to JSON. The output has the structure
    /// {"pages": [{"page": N, "objects": [{page, index, type, bbox, text,
    /// font, size, charBoxes}, ...]}, ...]}.
    static QString toJson(const std::vector<ObjectInfo>& objects);

private:
    const PDFDocument* m_document;
    const PDFFontCache* m_fontCache;
    const PDFCMS* m_cms;
    const PDFOptionalContentActivity* m_optionalContentActivity;
    const PDFMeshQualitySettings* m_meshQualitySettings;
};

}   // namespace pdf

#endif // PDFRECOGNIZETEXT_H
