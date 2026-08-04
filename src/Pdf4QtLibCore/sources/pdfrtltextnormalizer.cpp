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

#include "pdfrtltextnormalizer.h"

#include <QStringView>

namespace pdf
{

QString PDFRTLTextNormalizer::normalize(const QString& text, std::vector<int>* charMap)
{
    return normalize(text, Options(), charMap);
}

QString PDFRTLTextNormalizer::normalize(const QString& text, const Options& options, std::vector<int>* charMap)
{ // Step 1: NFKC — canonical composition AND compatibility folding, which
    // maps Arabic presentation forms (U+FB50..U+FDFF, U+FE70..U+FEFF) and the
    // lam-alef ligature (U+FEFB..U+FEFE) back to their base letter sequences.
    const QString composed = text.normalized(QString::NormalizationForm_KC);

    QString result;
    result.reserve(composed.size());
    if (charMap)
    {
        charMap->clear();
        charMap->reserve(composed.size());
    }

    // Step 6 (lam-alef collapse) needs lookahead over the raw composed stream.
    for (int i = 0; i < composed.size(); ++i)
    {
        const QChar ch = composed.at(i);
        const char32_t cp = ch.unicode();

        // Skip ZWNJ/ZWJ.
        if (options.stripJoiners && (cp == 0x200C || cp == 0x200D))
        {
            continue;
        }

        // Skip Arabic diacritics (tashkeel) and tatweel (kashida).
        const bool isDiacritic = (cp >= 0x064B && cp <= 0x0652) || cp == 0x0670 || cp == 0x0640;
        if (options.stripDiacritics && isDiacritic)
        {
            continue;
        }

        // Lam-alef: U+0644 U+0627 -> single lam. NFKC already converted the
        // presentation ligature (FEFB) into this two-char sequence, and PDF
        // ToUnicode maps often degrade the ligature to lam alone, so searching
        // "لا" must also match extracted "ل".
        if (options.collapseLamAlef && cp == 0x0644 && i + 1 < composed.size() &&
            composed.at(i + 1).unicode() == 0x0627)
        {
            result.append(QChar(0x0644));
            if (charMap)
            {
                charMap->push_back(i + 1); // last original char of the pair
            }
            ++i; // consume the alef
            continue;
        }

        // Persian <-> Arabic letter unification.
        char32_t unified = cp;
        if (options.unifyPersianArabic)
        {
            switch (cp)
            {
            case 0x064A: // Arabic yeh -> Persian yeh
                unified = 0x06CC;
                break;
            case 0x0643: // Arabic keheh -> Persian keheh
                unified = 0x06A9;
                break;
            case 0x0623: // alef with hamza above
            case 0x0625: // alef with hamza below
            case 0x0622: // alef with madda
            case 0x0671: // alef wasla
                unified = 0x0627;
                break;
            case 0x0629: // teh marbuta -> heh
                unified = 0x0647;
                break;
            case 0x0649: // alef maksura -> yeh
                unified = 0x06CC;
                break;
            case 0x06C0: // heh with yeh above -> heh
                unified = 0x0647;
                break;
            default:
                break;
            }
        }

        // Digit unification: Persian ۰-۹ (U+06F0..U+06F9) and Arabic-Indic
        // ٠-٩ (U+0660..U+0669) -> Western 0-9.
        if (options.unifyDigits)
        {
            if (cp >= 0x06F0 && cp <= 0x06F9)
            {
                unified = cp - 0x06F0 + 0x0030;
            }
            else if (cp >= 0x0660 && cp <= 0x0669)
            {
                unified = cp - 0x0660 + 0x0030;
            }
        }

        result.append(QChar(unified));
        if (charMap)
        {
            charMap->push_back(i);
        }
    }

    if (charMap)
    {
        Q_ASSERT(charMap->size() == result.size());
    }
    return result;
}

} // namespace pdf
