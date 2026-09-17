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
#include "pdfrtltextnormalizer.h"

#include <QtTest>

#include <vector>

// R4.1/R4.2 seam test: PDFBidi owns the FriBidi logical<->visual conversion
// that the search engine and the normalizer previously duplicated. The
// lam-alef/digit folding assertions pin the call-site behaviour the seam must
// preserve (PDFRTLTextNormalizer), so the corpus expectations are unchanged.
class RtlBidiTest : public QObject
{
    Q_OBJECT

private slots:
    void test_directionForLanguage();
    void test_logicalToVisual_hebrew();
    void test_logicalToVisual_ltrIdentity();
    void test_roundTrip_hebrew();
    void test_roundTrip_mixedBidi();
    void test_visualToLogical_reversesVisualRtl();
    void test_visualToLogical_empty();
    void test_levelsAndRuns_mixedBidi();
    void test_normalizer_lamAlefCollapse();
    void test_normalizer_visualOrderLamAlefCollapse();
    void test_normalizer_digitFolding();
};

void RtlBidiTest::test_directionForLanguage()
{
    using pdf::PDFBidi;
    QVERIFY(PDFBidi::directionForLanguage(QStringLiteral("fa")) == PDFBidi::Direction::RightToLeft);
    QVERIFY(PDFBidi::directionForLanguage(QStringLiteral("ar")) == PDFBidi::Direction::RightToLeft);
    QVERIFY(PDFBidi::directionForLanguage(QStringLiteral("he")) == PDFBidi::Direction::RightToLeft);
    QVERIFY(PDFBidi::directionForLanguage(QStringLiteral("ur")) == PDFBidi::Direction::RightToLeft);
    QVERIFY(PDFBidi::directionForLanguage(QStringLiteral("en")) == PDFBidi::Direction::LeftToRight);
    QVERIFY(PDFBidi::directionForLanguage(QString()) == PDFBidi::Direction::LeftToRight);
}

void RtlBidiTest::test_logicalToVisual_hebrew()
{
    // Hebrew has no presentation-form shaping, so the visual order is a plain
    // reversal of the logical run: 'שלום' -> 'םולש'.
    QCOMPARE(pdf::PDFBidi::logicalToVisual(QString::fromUtf8("שלום")), QString::fromUtf8("םולש"));
}

void RtlBidiTest::test_logicalToVisual_ltrIdentity()
{
    // Pure LTR: FriBidi does no reordering and the input passes through.
    QCOMPARE(pdf::PDFBidi::logicalToVisual(QStringLiteral("Hello world")), QStringLiteral("Hello world"));
}

void RtlBidiTest::test_roundTrip_hebrew()
{
    const QString logical = QString::fromUtf8("שלום");
    QCOMPARE(pdf::PDFBidi::visualToLogical(pdf::PDFBidi::logicalToVisual(logical)), logical);
}

void RtlBidiTest::test_roundTrip_mixedBidi()
{
    // The mixed run keeps its Latin prefix/suffix in place and reverses only
    // the Hebrew run; vis2log must restore the exact logical string.
    const QString logical = QString::fromUtf8("abc שלום def");
    QCOMPARE(pdf::PDFBidi::visualToLogical(pdf::PDFBidi::logicalToVisual(logical)), logical);
}

void RtlBidiTest::test_visualToLogical_reversesVisualRtl()
{
    // Same base-letter vectors as the pre-seam normalizer test: the writer
    // stores glyphs leftmost-first, so 'سلام' extracts as 'مالس'.
    QCOMPARE(pdf::PDFBidi::visualToLogical(QString::fromUtf8("مالس")), QString::fromUtf8("سلام"));
    QCOMPARE(pdf::PDFBidi::visualToLogical(QString::fromUtf8("ابحرم")), QString::fromUtf8("مرحبا"));
    QCOMPARE(pdf::PDFBidi::visualToLogical(QString::fromUtf8("םולש")), QString::fromUtf8("שלום"));
}

void RtlBidiTest::test_visualToLogical_empty()
{
    QVERIFY(pdf::PDFBidi::visualToLogical(QString()).isEmpty());
    QVERIFY(pdf::PDFBidi::logicalToVisual(QString()).isEmpty());
}

void RtlBidiTest::test_levelsAndRuns_mixedBidi()
{
    // "Hello " is LTR (levels 0), the two Arabic words form one RTL run
    // (odd levels); the split must be a single 6/9 boundary.
    const QString mixed = QString::fromUtf8("Hello سلام دنیا");
    const pdf::PDFBidi::Levels levels = pdf::PDFBidi::levels(mixed, pdf::PDFBidi::Direction::LeftToRight);
    const std::vector<pdf::PDFBidi::Run> runs = pdf::PDFBidi::runsFromLevels(levels.levels);
    QCOMPARE(runs.size(), size_t(2));
    QVERIFY(!runs[0].isRTL);
    QCOMPARE(runs[0].begin, size_t(0));
    QCOMPARE(runs[0].end, size_t(6));
    QVERIFY(runs[1].isRTL);
    QCOMPARE(runs[1].begin, size_t(6));
    QCOMPARE(runs[1].end, size_t(mixed.size()));

    // Pure LTR collapses to a single LTR run.
    const pdf::PDFBidi::Levels ltr =
        pdf::PDFBidi::levels(QStringLiteral("Hello world"), pdf::PDFBidi::Direction::LeftToRight);
    const std::vector<pdf::PDFBidi::Run> ltrRuns = pdf::PDFBidi::runsFromLevels(ltr.levels);
    QCOMPARE(ltrRuns.size(), size_t(1));
    QVERIFY(!ltrRuns.front().isRTL);
}

void RtlBidiTest::test_normalizer_lamAlefCollapse()
{
    // Logical lam-alef (U+0644 U+0627) collapses to a single lam; NFKC first
    // folds the presentation ligature (U+FEFB) to that same pair.
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString::fromUtf8("سلام")), QString::fromUtf8("سلم"));
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString(QChar(0xFEFB))), QString(QChar(0x0644)));
}

void RtlBidiTest::test_normalizer_visualOrderLamAlefCollapse()
{
    // In visual order the ligature appears reversed (U+0627 U+0644), so
    // 'مالس' must collapse the same way 'سلام' does in logical order.
    pdf::PDFRTLTextNormalizer::Options visualOptions;
    visualOptions.visualOrder = true;
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString::fromUtf8("مالس"), visualOptions), QString::fromUtf8("ملس"));
}

void RtlBidiTest::test_normalizer_digitFolding()
{
    // Persian (U+06F0..U+06F9) and Arabic-Indic (U+0660..U+0669) digits both
    // fold to Western digits, including a mixed run.
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString::fromUtf8("۱۲۳")), QStringLiteral("123"));
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString::fromUtf8("١٢٣")), QStringLiteral("123"));
    QCOMPARE(pdf::PDFRTLTextNormalizer::normalize(QString::fromUtf8("۱۲٣")), QStringLiteral("123"));
}

QTEST_GUILESS_MAIN(RtlBidiTest)

#include "tst_rtlbiditest.moc"
