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

#include "pdftoolabstractapplication.h"

namespace pdftool
{

/// form-fill command: set the values of interactive form fields.
///
/// Writes a new document. Each --field/--value pair names a field (fully
/// qualified name as reported by form-list) and the value to set. Text
/// fields take strings, buttons take On/Off, choice fields take the
/// selected option. The field appearance is regenerated so the new value
/// is visible when rendered.
class PDF4QTLIBCORESHARED_EXPORT PDFToolFormFill : public PDFToolAbstractApplication
{
public:
    explicit PDFToolFormFill() = default;

    QString getStandardString(StandardString standardString) const override;
    Options getOptionsFlags() const override;
    int execute(const PDFToolOptions& options) override;
};

} // namespace pdftool
