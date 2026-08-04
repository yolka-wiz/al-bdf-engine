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

#ifndef PDFTOOLDELETEOBJECT_H
#define PDFTOOLDELETEOBJECT_H

#include "pdftoolabstractapplication.h"

namespace pdftool
{

/// Delete a whole content object (text run, image, path) from a page of a
/// PDF document and write the result back into the content stream.
///
/// The object is addressed by its 0-based index on the page, which is the
/// index space emitted by the 'recognize-text' command.
class PDFToolDeleteObject : public PDFToolAbstractApplication
{
public:
    virtual QString getStandardString(StandardString standardString) const override;
    virtual int execute(const PDFToolOptions& options) override;
    virtual Options getOptionsFlags() const override;
};

} // namespace pdftool

#endif // PDFTOOLDELETEOBJECT_H
