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

/// sign command: apply a digital signature (PKCS#7 detached, adbe.pkcs7.detached).
///
/// Creates a signature form field and signs the document with the private
/// key from a PKCS#12 (.p12/.pfx) certificate file. The signature is
/// invisible by default; --page/--x/--y/--w/--h place a visible signature
/// widget (the certificate subject is drawn into it). The signed output
/// verifies with `albdf verify-signatures`.
class PDF4QTLIBCORESHARED_EXPORT PDFToolSign : public PDFToolAbstractApplication
{
public:
    explicit PDFToolSign() = default;

    QString getStandardString(StandardString standardString) const override;
    Options getOptionsFlags() const override;
    int execute(const PDFToolOptions& options) override;
};

} // namespace pdftool
