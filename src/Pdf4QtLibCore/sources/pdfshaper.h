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

#pragma once

#include "pdfglobal.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace pdf
{

/// Internal HarfBuzz shaping seam (roadmap R4).
///
/// Owns the `hb_blob`/`hb_face`/`hb_font` lifetime and the only `<hb.h>`
/// include in the core, so the RTL engines never touch the C API. It returns
/// shaped glyphs with the data the writer needs: glyph ids, nominal hmtx
/// advances, GPOS offsets, and clusters.
///
/// Clusters are ABSOLUTE UTF-16 indices into the whole text because runs are
/// added with `item_offset = run.begin` (docs/PROBLEMS.md S#2): callers must
/// NOT re-add `run.begin` to a cluster.
class PDF4QTLIBCORESHARED_EXPORT PDFShaper
{
public:
    /// One shaped glyph.
    struct Glyph
    {
        std::uint32_t glyphId = 0; ///< HarfBuzz glyph id (GID)
        std::uint32_t cluster = 0; ///< ABSOLUTE UTF-16 index into the full text
        PDFReal xAdvance = 0.0;    ///< hmtx horizontal advance (font units)
        PDFReal xOffset = 0.0;     ///< GPOS mark x offset (font units)
        PDFReal yOffset = 0.0;     ///< GPOS mark y offset (font units)
    };

    /// A directional run of the logical text to shape.
    struct RunInput
    {
        bool isRTL = false;    ///< RTL runs shape with `HB_DIRECTION_RTL`
        std::size_t begin = 0; ///< first UTF-16 unit of the run (inclusive)
        std::size_t end = 0;   ///< one past the run's last UTF-16 unit
    };

    /// Creates a shaper for \p fontData scaled to \p upem.
    ///
    /// \param fontData TrueType/OpenType font program bytes (kept alive by the
    ///        caller for the lifetime of the shaper).
    /// \param upem font units per em, used as the HarfBuzz scale.
    /// \returns a ready-to-use shaper; never null.
    static std::unique_ptr<PDFShaper> create(const QByteArray& fontData, PDFInteger upem);

    PDFShaper();
    ~PDFShaper();
    PDFShaper(const PDFShaper&) = delete;
    PDFShaper& operator=(const PDFShaper&) = delete;

    /// Shapes the run `text[run.begin, run.end)`.
    ///
    /// The buffer uses `HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS` so every
    /// input character (including combining marks) keeps its own cluster,
    /// which the ToUnicode mapping relies on (tashkeel-tolerant search).
    ///
    /// \param text full logical-order UTF-16 text (clusters are absolute).
    /// \param run logical range to shape, plus its direction.
    /// \param scriptTag 4-char ISO-15924 tag ("Arab", "Hebr").
    /// \param language document language code ("" = no language set).
    /// \returns the shaped glyphs in HarfBuzz order.
    std::vector<Glyph> shape(const std::vector<std::uint16_t>& text,
                             const RunInput& run,
                             const QByteArray& scriptTag,
                             const QString& language) const;

    /// True when \p glyphId's ink sits above its origin (y-up extents with
    /// `y_bearing > 0`); this decides the sign of the PDF `Ts` rise for marks.
    ///
    /// \param glyphId HarfBuzz glyph id to query.
    /// \returns true when the ink is above the origin (fatha-like), false when
    ///          below (kasra-like). Defaults to true when the query fails.
    bool glyphInkAboveOrigin(std::uint32_t glyphId) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace pdf
