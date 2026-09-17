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

#ifndef PDFPAGECONTENTREWRITER_H
#define PDFPAGECONTENTREWRITER_H

#include "pdfglobal.h"
#include "pdfobject.h"

#include <QByteArray>
#include <QString>

namespace pdf
{

class PDFDocument;
class PDFDocumentBuilder;

/// Shared write-back bridge for the CLI commands that rewrite one page's
/// content stream (`add-text` LTR/RTL and `delete-object`).
///
/// The three commands used to carry near-identical copies of the same
/// modifier/builder/compress/merge/write sequence, with subtle differences in
/// how the new stream is attached to the page. This helper factors that
/// sequence out and keeps the two attachment modes explicit:
///
/// - ContentsMode::Replace re-serializes the whole page elsewhere and installs
///   the supplied content bytes as the page's only /Contents stream.
/// - ContentsMode::Append keeps the existing /Contents untouched and appends
///   the supplied content bytes as one more stream.
///
/// The helper never parses page content, so Append carries no dependency on the
/// content editor; callers that need Replace are the ones that already parsed
/// and re-serialized the page.
class PDF4QTLIBCORESHARED_EXPORT PDFPageContentRewriter
{
public:
    /// How the new content stream is combined with the page's /Contents.
    enum class ContentsMode
    {
        /// The new stream replaces the page /Contents (add-text LTR, delete-object).
        Replace,
        /// The new stream is appended after the existing /Contents (add-text RTL).
        Append
    };

    /// Stage at which a rewrite failed. The CLI maps this to its own
    /// (translatable) stderr message, so the helper stays translation-free.
    enum class Failure
    {
        None,
        Finalize,
        Write
    };

    /// Outcome of rewrite().
    struct Result
    {
        /// Failing stage; Failure::None means the document was written.
        Failure failure = Failure::None;
        /// Writer error text; only meaningful when failure is Failure::Write.
        QString errorMessage;

        /// \returns true when the document was written to disk.
        bool isSuccess() const
        {
            return failure == Failure::None;
        }
    };

    /// Inputs describing one page write-back.
    struct Settings
    {
        /// Page whose content stream(s) are rewritten.
        PDFObjectReference pageReference;
        /// Font resources of the new content (also merged with existing page fonts).
        PDFDictionary fontDictionary;
        /// XObject resources of the new content (Replace mode only).
        PDFDictionary xobjectDictionary;
        /// ExtGState resources of the new content (Replace mode only).
        PDFDictionary graphicStateDictionary;
        /// Uncompressed bytes of the new content stream.
        QByteArray contentBytes;
        /// Replace vs. append the page /Contents.
        ContentsMode mode = ContentsMode::Replace;
        /// Output PDF file the modified document is written to.
        QString outputPath;
    };

    /// Rewrites the page's content stream(s) and writes the whole document to
    /// Settings::outputPath.
    ///
    /// \param document Document to modify (unmodified on failure).
    /// \param settings Page reference, resource dictionaries, content bytes,
    ///        attachment mode and output path.
    /// \returns the failing stage and (for write failures) the writer message;
    ///          Result::isSuccess() is true when the document was written.
    static Result rewrite(PDFDocument& document, const Settings& settings);

private:
    /// Inlines an indirect page /Resources into the page dictionary so the
    /// subsequent merge() preserves existing (non-overridden) resources:
    /// merge() replaces a referenced left object with a dictionary right one
    /// (see pdfobject.cpp), which would silently drop them.
    static void resolveIndirectResources(PDFDocumentBuilder* builder, PDFObject& pageObject);
    /// Resolves the page's existing /Font dictionary and adds every key that
    /// \p fontDictionary does not define yet (new font wins on key collision).
    static void
    mergeExistingFonts(const PDFDocumentBuilder* builder, const PDFObject& pageObject, PDFDictionary& fontDictionary);
    /// Compresses \p uncompressedContent and wraps it in a FlateDecode stream.
    static PDFObject buildContentStream(const QByteArray& uncompressedContent);
    static void applyReplace(PDFDocumentBuilder* builder, const Settings& settings);
    static void applyAppend(PDFDocumentBuilder* builder, const Settings& settings);
};

} // namespace pdf

#endif // PDFPAGECONTENTREWRITER_H
