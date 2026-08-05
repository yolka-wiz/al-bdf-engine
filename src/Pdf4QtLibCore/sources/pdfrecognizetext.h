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

#ifndef PDFRECOGNIZETEXT_H
#define PDFRECOGNIZETEXT_H

#include "pdfglobal.h"

#include <QRectF>
#include <QString>

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
/// the albdf 'recognize-text' CLI command.
///
/// Index contract: the \p index emitted for each object is the 0-based index
/// of the element in the PDFEditedPageContent element list produced by
/// PDFPageContentEditorProcessor for that page (in content stream order).
/// The same index space is consumed by the albdf 'delete-object' command,
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

} // namespace pdf

#endif // PDFRECOGNIZETEXT_H
