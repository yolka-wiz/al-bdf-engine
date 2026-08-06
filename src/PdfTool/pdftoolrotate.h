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

#ifndef PDFTOOLROTATE_H
#define PDFTOOLROTATE_H

#include "pdftoolabstractapplication.h"

namespace pdftool
{

/// Rotate pages of a PDF document by 90, 180 or 270 degrees clockwise.
///
/// Rotation is applied through the /Rotate page attribute (combined with any
/// existing page rotation), so page content, resources and annotations are
/// preserved; only the display orientation changes. The output document is
/// written byte-deterministically (no timestamps, no random IDs).
class PDFToolRotate : public PDFToolAbstractApplication
{
public:
    virtual QString getStandardString(StandardString standardString) const override;
    virtual int execute(const PDFToolOptions& options) override;
    virtual Options getOptionsFlags() const override;
};

} // namespace pdftool

#endif // PDFTOOLROTATE_H
