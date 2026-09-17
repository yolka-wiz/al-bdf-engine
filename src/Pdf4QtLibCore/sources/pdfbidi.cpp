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

#include "pdfbidi.h"

#include <fribidi.h>

namespace pdf
{

namespace
{

/// Converts a QString into the UTF-16 code-unit buffer FriBidi consumes.
/// UTF-16 units are passed through as-is (surrogate pairs are not combined),
/// which is the behaviour the engines shipped with.
std::vector<FriBidiChar> toFriBidiChars(const QString& text)
{
    std::vector<FriBidiChar> chars(static_cast<std::size_t>(text.size()));
    for (int i = 0; i < text.size(); ++i)
    {
        chars[static_cast<std::size_t>(i)] = static_cast<FriBidiChar>(text.at(i).unicode());
    }
    return chars;
}

FriBidiParType toFriBidiParType(PDFBidi::Direction direction)
{
    switch (direction)
    {
    case PDFBidi::Direction::LeftToRight:
        return FRIBIDI_PAR_LTR;
    case PDFBidi::Direction::RightToLeft:
        return FRIBIDI_PAR_RTL;
    case PDFBidi::Direction::Auto:
    default:
        return FRIBIDI_PAR_ON;
    }
}

QString fromFriBidiChars(const std::vector<FriBidiChar>& chars)
{
    QString result;
    result.reserve(static_cast<int>(chars.size()));
    for (const FriBidiChar c : chars)
    {
        result.append(QChar(static_cast<ushort>(c)));
    }
    return result;
}

} // namespace

PDFBidi::Direction PDFBidi::directionForLanguage(const QString& language)
{
    if (language == QLatin1String("fa") || language == QLatin1String("ar") || language == QLatin1String("he") ||
        language == QLatin1String("ur"))
    {
        return Direction::RightToLeft;
    }
    return Direction::LeftToRight;
}

PDFBidi::Levels PDFBidi::levels(const QString& text, Direction direction)
{
    Levels result;

    const std::vector<FriBidiChar> logical = toFriBidiChars(text);
    std::vector<FriBidiChar> visual(logical.size());
    std::vector<FriBidiStrIndex> positionsLToV(logical.size());
    std::vector<FriBidiStrIndex> positionsVToL(logical.size());
    std::vector<FriBidiLevel> embeddingLevels(logical.size());

    FriBidiParType baseDirection = toFriBidiParType(direction);
    result.maxLevel = int(fribidi_log2vis(logical.data(),
                                          FriBidiStrIndex(logical.size()),
                                          &baseDirection,
                                          visual.data(),
                                          positionsLToV.data(),
                                          positionsVToL.data(),
                                          embeddingLevels.data()));
    result.baseRTL = (baseDirection == FRIBIDI_PAR_RTL || baseDirection == FRIBIDI_PAR_WRTL);

    result.levels.resize(embeddingLevels.size());
    for (std::size_t i = 0; i < embeddingLevels.size(); ++i)
    {
        result.levels[i] = int(embeddingLevels[i]);
    }
    return result;
}

std::vector<PDFBidi::Run> PDFBidi::runsFromLevels(const std::vector<int>& levels)
{
    std::vector<Run> runs;
    for (std::size_t i = 0; i < levels.size();)
    {
        const bool rtl = (levels[i] & 1) != 0;
        std::size_t j = i + 1;
        while (j < levels.size() && ((levels[j] & 1) != 0) == rtl)
        {
            ++j;
        }
        runs.push_back(Run{i, j, rtl});
        i = j;
    }
    return runs;
}

QString PDFBidi::logicalToVisual(const QString& logical, Direction direction)
{
    if (logical.isEmpty())
    {
        return QString();
    }

    const std::vector<FriBidiChar> logicalChars = toFriBidiChars(logical);
    std::vector<FriBidiChar> visual(logicalChars.size());
    std::vector<FriBidiStrIndex> positionsLToV(logicalChars.size());
    std::vector<FriBidiStrIndex> positionsVToL(logicalChars.size());
    std::vector<FriBidiLevel> levels(logicalChars.size());

    FriBidiParType baseDirection = toFriBidiParType(direction);
    const FriBidiLevel maxLevel = fribidi_log2vis(logicalChars.data(),
                                                  FriBidiStrIndex(logicalChars.size()),
                                                  &baseDirection,
                                                  visual.data(),
                                                  positionsLToV.data(),
                                                  positionsVToL.data(),
                                                  levels.data());
    if (maxLevel == 0)
    {
        return logical;
    }
    return fromFriBidiChars(visual);
}

QString PDFBidi::visualToLogical(const QString& visual)
{
    if (visual.isEmpty())
    {
        return QString();
    }

    const std::vector<FriBidiChar> visualChars = toFriBidiChars(visual);
    std::vector<FriBidiCharType> bidiTypes(visualChars.size());
    std::vector<FriBidiLevel> levels(visualChars.size());
    std::vector<FriBidiStrIndex> positionsLToV(visualChars.size());

    fribidi_get_bidi_types(visualChars.data(), FriBidiStrIndex(visualChars.size()), bidiTypes.data());

    // Auto-detect the base direction from the visual content, mirroring the
    // search engine's logicalToVisual (FRIBIDI_PAR_ON).
    FriBidiParType baseDir = FRIBIDI_PAR_ON;
    const FriBidiLevel maxLevel = fribidi_get_par_embedding_levels_ex(
        bidiTypes.data(), nullptr, FriBidiStrIndex(visualChars.size()), &baseDir, levels.data());
    if (maxLevel == 0)
    {
        // No reordering (pure LTR or error) — return the input unchanged.
        return visual;
    }

    for (std::size_t i = 0; i < positionsLToV.size(); ++i)
    {
        positionsLToV[i] = FriBidiStrIndex(i);
    }
    // The reorder permutation (the `map` output) is what we need; the
    // function's returned maximum level is not used here.
    const FriBidiLevel reorderMaxLevel = fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT,
                                                              bidiTypes.data(),
                                                              FriBidiStrIndex(visualChars.size()),
                                                              0,
                                                              baseDir,
                                                              levels.data(),
                                                              nullptr,
                                                              positionsLToV.data());
    (void)reorderMaxLevel;

    std::vector<FriBidiChar> reordered(visualChars.size());
    for (std::size_t i = 0; i < visualChars.size(); ++i)
    {
        reordered[i] = visualChars[static_cast<std::size_t>(positionsLToV[i])];
    }
    return fromFriBidiChars(reordered);
}

} // namespace pdf
