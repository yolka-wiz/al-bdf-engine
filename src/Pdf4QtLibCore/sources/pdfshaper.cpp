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

#include "pdfshaper.h"

#include <hb-ot.h>
#include <hb.h>

namespace pdf
{

struct PDFShaper::Impl
{
    hb_blob_t* blob = nullptr;
    hb_face_t* face = nullptr;
    hb_font_t* font = nullptr;
};

std::unique_ptr<PDFShaper> PDFShaper::create(const QByteArray& fontData, PDFInteger upem)
{
    std::unique_ptr<PDFShaper> shaper = std::make_unique<PDFShaper>();
    shaper->m_impl->blob =
        hb_blob_create(fontData.constData(), unsigned(fontData.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    shaper->m_impl->face = hb_face_create(shaper->m_impl->blob, 0);
    shaper->m_impl->font = hb_font_create(shaper->m_impl->face);
    hb_ot_font_set_funcs(shaper->m_impl->font);
    hb_font_set_scale(shaper->m_impl->font, int(upem), int(upem));
    return shaper;
}

PDFShaper::PDFShaper() : m_impl(std::make_unique<Impl>()) {}

PDFShaper::~PDFShaper()
{
    if (m_impl->font)
    {
        hb_font_destroy(m_impl->font);
    }
    if (m_impl->face)
    {
        hb_face_destroy(m_impl->face);
    }
    if (m_impl->blob)
    {
        hb_blob_destroy(m_impl->blob);
    }
}

std::vector<PDFShaper::Glyph> PDFShaper::shape(const std::vector<std::uint16_t>& text,
                                               const RunInput& run,
                                               const QByteArray& scriptTag,
                                               const QString& language) const
{
    std::vector<Glyph> glyphs;
    if (run.begin >= run.end)
    {
        return glyphs;
    }

    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_set_direction(buffer, run.isRTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, hb_script_from_string(scriptTag.constData(), -1));
    hb_language_t hbLanguage = hb_language_from_string(language.toUtf8().constData(), -1);
    if (hbLanguage)
    {
        hb_buffer_set_language(buffer, hbLanguage);
    }
    hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
    hb_buffer_add_utf16(buffer, text.data(), int(text.size()), int(run.begin), int(run.end - run.begin));
    hb_shape(m_impl->font, buffer, nullptr, 0);

    unsigned glyphCount = 0;
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    glyphs.reserve(glyphCount);
    for (unsigned g = 0; g < glyphCount; ++g)
    {
        Glyph glyph;
        glyph.glyphId = glyphInfo[g].codepoint;
        glyph.cluster = glyphInfo[g].cluster;
        // /W advance: hmtx advance, nominal rather than the shaped advance.
        glyph.xAdvance = PDFReal(hb_font_get_glyph_h_advance(m_impl->font, glyph.glyphId));
        glyph.xOffset = PDFReal(glyphPos[g].x_offset);
        glyph.yOffset = PDFReal(glyphPos[g].y_offset);
        glyphs.push_back(glyph);
    }

    hb_buffer_destroy(buffer);
    return glyphs;
}

bool PDFShaper::glyphInkAboveOrigin(std::uint32_t glyphId) const
{
    hb_glyph_extents_t extents = {};
    if (hb_font_get_glyph_extents(m_impl->font, glyphId, &extents) != 0)
    {
        return extents.y_bearing > 0;
    }
    return true;
}

} // namespace pdf
