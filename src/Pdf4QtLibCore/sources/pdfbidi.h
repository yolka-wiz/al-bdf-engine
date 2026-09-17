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

#include <QString>

#include <cstddef>
#include <vector>

namespace pdf
{

/// Internal bidi backend seam (roadmap R4).
///
/// Owns the only `<fribidi.h>` include in the core so the RTL engines talk to
/// this class instead of the C API. It exposes exactly what the three RTL call
/// sites need: logical<->visual reordering, base-direction resolution, and the
/// embedding-level/runs view used by the writer.
///
/// Documented behaviour that must not change:
///  - **R#2** the writer does NOT reverse HarfBuzz >= 4 RTL glyphs; this seam
///    only produces embedding levels for run splitting, it never mirrors text.
///  - **R#3** `logicalToVisual` preserves FriBidi's Arabic presentation-form
///    shaping; callers that compare against base-letter text must fold the
///    result back with an NFKC pass (`PDFRTLTextNormalizer::normalize`).
///  - **S#3** the lam-alef ligature is collapsed by the normalizer, not here.
class PDF4QTLIBCORESHARED_EXPORT PDFBidi
{
public:
    /// Paragraph base direction. `Auto` lets FriBidi resolve the direction
    /// from the first strong character (`FRIBIDI_PAR_ON`); the writer instead
    /// picks an explicit direction from the document language.
    enum class Direction
    {
        Auto,
        LeftToRight,
        RightToLeft
    };

    /// One maximal directional run in LOGICAL order, derived from embedding
    /// level parity: odd level = RTL, even level = LTR.
    struct Run
    {
        std::size_t begin = 0; ///< first UTF-16 unit of the run (inclusive)
        std::size_t end = 0;   ///< one past the run's last UTF-16 unit
        bool isRTL = false;    ///< the run's embedding level is odd
    };

    /// Per-character embedding levels plus the resolved paragraph direction.
    struct Levels
    {
        std::vector<int> levels; ///< one embedding level per UTF-16 code unit
        bool baseRTL = false;    ///< resolved paragraph direction
        /// FriBidi's maximum embedding level. The writer treats 0 as failure,
        /// matching the pre-seam `if (!fribidi_log2vis(...))` check.
        int maxLevel = 0;
    };

    /// Maps a document language code to an explicit base direction. The RTL
    /// languages are "fa", "ar", "he" and "ur"; everything else is LTR.
    ///
    /// \param language document language code (may be empty).
    /// \returns the base direction to shape with.
    static Direction directionForLanguage(const QString& language);

    /// Computes the embedding levels of \p text in LOGICAL order.
    ///
    /// UTF-16 code units are passed through as `FriBidiChar` unchanged (the
    /// historical engine behaviour; surrogate pairs are not combined).
    ///
    /// \param text logical-order input text.
    /// \param direction paragraph direction to run the algorithm with.
    /// \returns the levels, the resolved direction, and `maxLevel`.
    static Levels levels(const QString& text, Direction direction);

    /// Splits embedding \p levels into maximal runs of equal parity.
    ///
    /// \param levels one embedding level per UTF-16 unit, in logical order.
    /// \returns runs in logical order; adjacent runs always alternate RTL.
    static std::vector<Run> runsFromLevels(const std::vector<int>& levels);

    /// Reorders logical \p text to VISUAL order (`fribidi_log2vis`).
    ///
    /// Arabic text is shaped into presentation forms by FriBidi, so callers
    /// that match base letters must NFKC-fold the result (R#3). Returns
    /// \p logical unchanged when FriBidi performs no reordering
    /// (`maxLevel == 0`); the search engine relies on that identity for
    /// pure-LTR queries.
    ///
    /// \param logical logical-order text.
    /// \param direction base direction; `Auto` auto-detects it.
    /// \returns the visual-order string.
    static QString logicalToVisual(const QString& logical, Direction direction = Direction::Auto);

    /// Reorders visual \p text back to LOGICAL order.
    ///
    /// Emulates the `fribidi_vis2log` primitive removed in FriBidi 1.0: it
    /// computes the bidi types and embedding levels of the visual string and
    /// applies the L2/L3 reorder (`fribidi_reorder_line`). The reorder
    /// permutation is its own inverse for the string's own levels, so
    /// reordering the visual string recovers the logical order. Returns
    /// \p visual unchanged for pure-LTR input.
    ///
    /// \param visual visual-order text (as extracted from a PDF).
    /// \returns the logical-order string.
    static QString visualToLogical(const QString& visual);
};

} // namespace pdf
