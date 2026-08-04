// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (c) 2026 pdfedit contributors
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
// This file is part of the pdfedit project, a fork of PDF4QT (MIT).
// The upstream PDF4QT portions remain under the MIT License; see the
// upstream copyright headers and the LICENSE file.

#pragma once

#include "pdftoolabstractapplication.h"

namespace pdftool
{

/// form-list command: enumerate the interactive form fields of a document
/// (names, types, values, widget pages and rectangles).
///
/// Read-only. This is the CLI companion to form-fill: it tells you what
/// fields exist so you can fill them. Upstream PDF4QT only exposed forms
/// through its GUI; this makes the same PDFForm machinery scriptable.
class PDF4QTLIBCORESHARED_EXPORT PDFToolFormList : public PDFToolAbstractApplication
{
public:
    explicit PDFToolFormList() = default;

    QString getStandardString(StandardString standardString) const override;
    Options getOptionsFlags() const override;
    int execute(const PDFToolOptions& options) override;
};

} // namespace pdftool
