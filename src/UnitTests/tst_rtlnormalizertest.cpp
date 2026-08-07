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

#include "pdfrtltextnormalizer.h"

#include <QtTest>

class RtlNormalizerTest : public QObject
{
    Q_OBJECT

private slots:
    void test_invertToLogical_reversesVisualRtl();
    void test_invertToLogical_keepsLtrUntouched();
    void test_invertToLogical_mixedBidi();
    void test_invertToLogical_rtlWithDigits();
    void test_invertToLogical_empty();
};

void RtlNormalizerTest::test_invertToLogical_reversesVisualRtl()
{
    // The RTL writer stores glyphs leftmost-first (visual order) and the
    // /ActualText payload it emits is built in that same glyph order, so the
    // fixture's logical 'سلام' extracts as 'مالس' (verified: `albdf
    // fetch-text gui-rtl.pdf` -> 'مالس'). The GUI clipboard path must
    // re-invert to logical order.
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QString::fromUtf8("مالس")), QString::fromUtf8("سلام"));
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QString::fromUtf8("ابحرم")), QString::fromUtf8("مرحبا"));
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QString::fromUtf8("םולש")), QString::fromUtf8("שלום"));
}

void RtlNormalizerTest::test_invertToLogical_keepsLtrUntouched()
{
    // Pure LTR text has no bidi reordering and must pass through unchanged.
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QStringLiteral("Hello world")), QStringLiteral("Hello world"));
}

void RtlNormalizerTest::test_invertToLogical_mixedBidi()
{
    // LTR-leading mixed text: the RTL run is reversed in place.
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QString::fromUtf8("abc مالس def")), QString::fromUtf8("abc سلام def"));
}

void RtlNormalizerTest::test_invertToLogical_rtlWithDigits()
{
    // 'سلام 123' renders with the digits leftmost, so the visual-order
    // string is '123 مالس'; inversion restores the logical order.
    QCOMPARE(pdf::PDFRTLTextNormalizer::invertToLogical(QString::fromUtf8("123 مالس")), QString::fromUtf8("سلام 123"));
}

void RtlNormalizerTest::test_invertToLogical_empty()
{
    QVERIFY(pdf::PDFRTLTextNormalizer::invertToLogical(QString()).isEmpty());
}

QTEST_GUILESS_MAIN(RtlNormalizerTest)

#include "tst_rtlnormalizertest.moc"
