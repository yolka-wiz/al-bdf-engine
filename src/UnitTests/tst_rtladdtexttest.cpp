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

// Integration test for the RTL add-text path (--rtl --font --lang).
//
// Runs albdf against the blank fixture with the bundled OFL fonts and verifies:
//   - RTL text is embedded as Type0/Identity-H (info-fonts lists it, embedded=Yes)
//   - Hebrew round-trips exactly through fetch-text (no ligatures/marks)
//   - Persian extracts with expected degradation (lam-alef ligature -> lam)
//   - the RTL font is registered under a separate key (F2) so LTR text (F1)
//     still works on the same page
//   - rendering the RTL page produces no render errors

#include <QtTest>

#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace
{
struct ToolResult
{
    int exitCode = -1;
    QByteArray stdoutData;
    QByteArray stderrData;
};

// Rows (y) that contain ink (any pixel darker than `threshold`) in `image`,
// ascending. Used to locate the vertical band a rendered text occupies.
QVector<int> inkRows(const QImage& image, int threshold = 200)
{
    QVector<int> rows;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (qGray(image.pixel(x, y)) < threshold)
            {
                rows.append(y);
                break;
            }
        }
    }
    return rows;
}

// Rows (y) where `other` differs from `base` by more than `delta` gray
// levels in at least one pixel. The two PDFs under comparison are byte-
// identical except for the diacritic mark, so the differing pixels are the
// mark's ink (plus sub-threshold antialiasing noise).
QVector<int> diffRows(const QImage& base, const QImage& other, int delta = 12)
{
    QVector<int> rows;
    const int width = qMin(base.width(), other.width());
    const int height = qMin(base.height(), other.height());
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (qAbs(qGray(base.pixel(x, y)) - qGray(other.pixel(x, y))) > delta)
            {
                rows.append(y);
                break;
            }
        }
    }
    return rows;
}

ToolResult runTool(const QString& toolPath, const QStringList& arguments, const QString& workingDir)
{
    ToolResult result;
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(workingDir);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(toolPath, arguments);
    if (!process.waitForStarted() || !process.waitForFinished(180000))
    {
        return result;
    }
    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    result.stderrData = process.readAllStandardError();
    return result;
}
} // namespace

class RtlAddTextTest : public QObject
{
    Q_OBJECT

private slots:
    void test_hebrewRoundTrip();
    void test_persianExtraction();
    void test_lamAlefFullLigatureExtraction();
    void test_decomposedYehNoDoubleExtraction();
    void test_rtlFontEmbedded();
    void test_rtlKeepsLtrIntact();
    void test_rtlRenderNoErrors();
    void test_rtlNotMirrored();
    void test_verticalMarkOffsets();
};

void RtlAddTextTest::test_hebrewRoundTrip()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("שלום עולם"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_HEBREW),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("he")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    // PDF content streams store VISUAL order: RTL text extracts in
    // right-to-left reading order (entire line reversed: "שלום עולם" ->
    // "םלוע םולש"). Hebrew has no ligatures or decomposed marks, so the
    // reversal is exact.
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("םלוע םולש")),
             "Hebrew text must extract in visual order (reversed for RTL)");
}

void RtlAddTextTest::test_persianExtraction()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("سلام دنیا"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    // RTL extracts in visual order: "سلام دنیا" -> reversed runs. The
    // /ActualText overlay (DB #21) restores the full lam-alef ligature, so
    // "سلام" -> visual "ملاس" (with ل+ا, not the degraded "ملس").
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("ایند ملاس")),
             "Persian text must extract in visual order with the full lam-alef ligature");
}

void RtlAddTextTest::test_lamAlefFullLigatureExtraction()
{
    // DB #21 (P1): the ToUnicode CMap can only carry one UTF-16 unit per
    // code, so a lam-alef ligature maps to its first letter ('ل') and
    // extraction used to degrade the visual glyph 'لا' to 'ل'. The engine
    // writes /ActualText (exact logical text) around RTL runs; extraction
    // must recover the full ligature from it.
    //
    // Logical "علا" (ع ل ا) shapes to two glyphs in visual order:
    // [lam-alef ligature, ع]. Extraction must yield "لاع" — the full
    // ligature at its visual position — NOT the degraded "لع".
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/la.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("علا"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    const QString fetched = QString::fromUtf8(fetchResult.stdoutData);
    QVERIFY2(fetched.contains(QString::fromUtf8("لاع")),
             "extraction must recover the full lam-alef ligature (لا) at its visual position");
    QVERIFY2(!fetched.contains(QString::fromUtf8("لع")),
             "extraction must NOT degrade the lam-alef ligature to its first letter");
}

void RtlAddTextTest::test_decomposedYehNoDoubleExtraction()
{
    // DB #22 (P2): Arabic yeh (U+064A) decomposes into base + dot; both glyphs
    // share the base's cluster, so extraction used to emit the yeh twice —
    // 'عليكم' came back as 'علييكم'. The /ActualText overlay (DB #21) carries
    // the exact logical text, so fetch-text must return the single yeh.
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/yeh.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("عليكم"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    const QString fetched = QString::fromUtf8(fetchResult.stdoutData);
    QVERIFY2(!fetched.contains(QString::fromUtf8("يي")),
             "extraction must NOT duplicate the decomposed yeh (علييكم artifact)");
    QVERIFY2(fetched.contains(QString::fromUtf8("ي")), "extraction must still contain the single yeh of عليكم");
}

void RtlAddTextTest::test_rtlFontEmbedded()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("سلام"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult infoResult = runTool(toolPath, {QStringLiteral("info-fonts"), outputPath}, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);
    QVERIFY2(infoResult.stdoutData.contains("Vazirmatn"), "RTL font must be embedded");
    QVERIFY2(infoResult.stdoutData.contains("Type 0"), "RTL font must be Type0/CID");
    QVERIFY2(infoResult.stdoutData.contains("Yes"), "RTL font must be marked embedded");
}

void RtlAddTextTest::test_rtlKeepsLtrIntact()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // RTL first, then LTR on the same page.
    const QString rtlPath = tmpDir.path() + QStringLiteral("/rtl.pdf");
    ToolResult rtlResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    rtlPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("سلام"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(rtlResult.exitCode, 0);

    const QString mixedPath = tmpDir.path() + QStringLiteral("/mixed.pdf");
    ToolResult ltrResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    rtlPath,
                                    mixedPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("600"),
                                    QStringLiteral("--text"),
                                    QStringLiteral("LTR still OK"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("16")},
                                   tmpDir.path());
    QCOMPARE(ltrResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), mixedPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains("LTR still OK"), "LTR text must survive RTL add");
    // RTL extracts in visual order: "سلام" -> "ملاس" here (full lam-alef).
    // The /ActualText overlay (DB #21) plus the R#4 marked-content
    // preservation fix restore the full ligature even after a second
    // (LTR) add-text rewrites the page through the content editor.
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("ملاس")),
             "RTL text must survive LTR add (full ligature preserved by R#4 fix)");
}

void RtlAddTextTest::test_rtlRenderNoErrors()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("سلام دنیا"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_PERSIAN),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult renderResult = runTool(toolPath,
                                      {QStringLiteral("render"),
                                       outputPath,
                                       QStringLiteral("--page-first"),
                                       QStringLiteral("1"),
                                       QStringLiteral("--page-last"),
                                       QStringLiteral("1"),
                                       QStringLiteral("--image-format"),
                                       QStringLiteral("png"),
                                       QStringLiteral("--image-res-dpi"),
                                       QStringLiteral("72"),
                                       QStringLiteral("--image-output-dir"),
                                       tmpDir.path()},
                                      tmpDir.path());
    QCOMPARE(renderResult.exitCode, 0);
    // The "Rendering Errors" section header is always printed; check that no
    // actual error row follows it (an error row contains "Error" + a message).
    QVERIFY2(!renderResult.stdoutData.contains("FreeType"), "no FreeType errors in RTL render");
    QVERIFY2(!renderResult.stdoutData.contains(QByteArray("Error      ")), "RTL page must render without errors");
    QVERIFY2(QFile::exists(tmpDir.path() + QStringLiteral("/Image_1.png")), "render must produce an image");
}

void RtlAddTextTest::test_rtlNotMirrored()
{
    // Mirror-regression: with a correct content stream, RTL text extracts in
    // VISUAL order (entire line reversed). If someone reintroduces the fpdf2
    // #1802 glyph reversal, extraction returns LOGICAL order and this fails.
    // (Verified against Qt's bidi-aware QPainter reference: glyph positions
    // match only without the reversal — HarfBuzz >= 4 emits RTL runs
    // leftmost-first already.)
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    QString::fromUtf8(TEST_BLANK_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("אבג"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    QString::fromUtf8(TEST_FONT_HEBREW),
                                    QStringLiteral("--lang"),
                                    QStringLiteral("he")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("גבא")),
             "RTL must extract in visual order (אבג -> גבא); glyph reversal would mirror the render");
}

void RtlAddTextTest::test_verticalMarkOffsets()
{
    // P3 (RED): Arabic/Persian diacritics (tashkeel) must render ABOVE the
    // base letter, not at the baseline. pdfrtltextengine.cpp captures the
    // HarfBuzz GPOS yOffset per glyph (lines 356-357) but the emitter
    // discards it (lines 467-474: TJ arrays are horizontal-only), so a fatha
    // in "مَا" is painted at the baseline, overlapping the base letter, and a
    // kasra in "بِسْم" never drops below the baseline. The planned fix emits
    // a per-glyph text rise (`Ts`) for non-zero yOffset marks.
    //
    // This test pixel-probes the rendered page DIFFERENTIALLY: it renders the
    // base text with and without the mark (byte-identical PDFs except for the
    // mark glyph) and requires the mark's ink (the differing pixels) to sit
    // (a) in a band ABOVE the base text's top row and (b) NOT inside the base
    // letters' core band — and, for kasra, (c) BELOW the base text.
    //
    // Calibration (verified against the current render at 24pt/72dpi on the
    // blank fixture, text at (72,700) -> baseline at image row ~92):
    //   "ما" ink rows 75..91 (alef top at row 75)
    //   fatha diff ink rows 78..91  -> inside the base band: (a) and (b) FAIL
    //   "بسم" ink rows 75..91; kasra diff max row 92 -> nothing below: (c) FAIL
    // With the fix, the fatha (GPOS y_offset -146 font units -> ~1.7pt rise)
    // moves to rows ~74..77, i.e. above row 75 and clear of the core band.
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // add-text + render page 1 to PNG (72 dpi: 1 PDF pt == 1 px), returning
    // the PNG path (Image_1.png is renamed per case; each render overwrites
    // the shared output name).
    const auto addAndRender = [&](const QString& text, const QString& caseName) -> QString {
        const QString pdfPath = tmpDir.path() + QLatin1Char('/') + caseName + QStringLiteral(".pdf");
        const ToolResult addResult = runTool(toolPath,
                                             {QStringLiteral("add-text"),
                                              QString::fromUtf8(TEST_BLANK_PDF),
                                              pdfPath,
                                              QStringLiteral("--page"),
                                              QStringLiteral("1"),
                                              QStringLiteral("--x"),
                                              QStringLiteral("72"),
                                              QStringLiteral("--y"),
                                              QStringLiteral("700"),
                                              QStringLiteral("--text"),
                                              text,
                                              QStringLiteral("--size"),
                                              QStringLiteral("24"),
                                              QStringLiteral("--rtl"),
                                              QStringLiteral("--font"),
                                              QString::fromUtf8(TEST_FONT_PERSIAN),
                                              QStringLiteral("--lang"),
                                              QStringLiteral("fa")},
                                             tmpDir.path());
        if (addResult.exitCode != 0)
        {
            return QString();
        }
        const ToolResult renderResult = runTool(toolPath,
                                                {QStringLiteral("render"),
                                                 pdfPath,
                                                 QStringLiteral("--page-first"),
                                                 QStringLiteral("1"),
                                                 QStringLiteral("--page-last"),
                                                 QStringLiteral("1"),
                                                 QStringLiteral("--image-format"),
                                                 QStringLiteral("png"),
                                                 QStringLiteral("--image-res-dpi"),
                                                 QStringLiteral("72"),
                                                 QStringLiteral("--image-output-dir"),
                                                 tmpDir.path()},
                                                tmpDir.path());
        if (renderResult.exitCode != 0)
        {
            return QString();
        }
        const QString pngPath = tmpDir.path() + QStringLiteral("/Image_1.png");
        if (!QFile::exists(pngPath))
        {
            return QString();
        }
        const QString target = tmpDir.path() + QLatin1Char('/') + caseName + QStringLiteral(".png");
        QFile::rename(pngPath, target);
        return target;
    };

    // ------------------------------------------------------------------
    // Probe 1 — fatha above the base letter: "مَا" vs "ما".
    // ------------------------------------------------------------------
    const QString fathaBasePng = addAndRender(QString::fromUtf8("ما"), QStringLiteral("fatha_base"));
    const QString fathaMarkPng = addAndRender(QString::fromUtf8("مَا"), QStringLiteral("fatha_mark"));
    QVERIFY2(!fathaBasePng.isEmpty() && !fathaMarkPng.isEmpty(), "add-text/render must succeed for the fatha probe");

    const QImage fathaBase(fathaBasePng);
    const QImage fathaMarked(fathaMarkPng);
    QVERIFY2(!fathaBase.isNull() && !fathaMarked.isNull(), "fatha probe PNGs must load");

    const QVector<int> fathaBaseRows = inkRows(fathaBase);
    QVERIFY2(!fathaBaseRows.isEmpty(), "fatha base render must contain ink");
    const int fathaBaseTop = fathaBaseRows.first();
    const int fathaBaseBottom = fathaBaseRows.last();

    const QVector<int> fathaMarkRows = diffRows(fathaBase, fathaMarked);
    QVERIFY2(!fathaMarkRows.isEmpty(), "the fatha mark must produce visible ink difference vs the plain base text");

    // (a) The mark must have ink in the band ABOVE the base text's top row.
    // Base "ما" spans rows 75..91; a correctly placed fatha sits at ~74..77.
    // Today the fatha paints at the baseline: its diff ink spans rows 78..91
    // and the above-band is empty -> FAIL.
    const int aboveBandTop = fathaBaseTop - 6;
    const int aboveBandBottom = fathaBaseTop - 1;
    bool fathaInkAbove = false;
    for (int y : fathaMarkRows)
    {
        if (y >= aboveBandTop && y <= aboveBandBottom)
        {
            fathaInkAbove = true;
            break;
        }
    }
    QVERIFY2(
        fathaInkAbove,
        qPrintable(QStringLiteral("P3: fatha must render ABOVE the base letter (ink in rows %1..%2, base top row %3); "
                                  "today it paints at the baseline (diff ink rows %4..%5)")
                       .arg(aboveBandTop)
                       .arg(aboveBandBottom)
                       .arg(fathaBaseTop)
                       .arg(fathaMarkRows.first())
                       .arg(fathaMarkRows.last())));

    // (b) The mark must NOT overpaint the base letters' core band (the rows
    // between the base text's top and bottom margins). Today the fatha's ink
    // overlaps the core -> FAIL.
    const int coreTop = fathaBaseTop + 2;
    const int coreBottom = fathaBaseBottom - 2;
    bool fathaOverpaintsCore = false;
    for (int y : fathaMarkRows)
    {
        if (y >= coreTop && y <= coreBottom)
        {
            fathaOverpaintsCore = true;
            break;
        }
    }
    QVERIFY2(!fathaOverpaintsCore,
             qPrintable(QStringLiteral("P3: fatha must NOT overpaint the base letter core (rows %1..%2); today its ink "
                                       "overlaps (diff ink rows %3..%4)")
                            .arg(coreTop)
                            .arg(coreBottom)
                            .arg(fathaMarkRows.first())
                            .arg(fathaMarkRows.last())));

    // ------------------------------------------------------------------
    // Probe 2 — kasra below the baseline: "بِسْم" vs "بسم".
    // ------------------------------------------------------------------
    const QString kasraBasePng = addAndRender(QString::fromUtf8("بسم"), QStringLiteral("kasra_base"));
    const QString kasraMarkPng = addAndRender(QString::fromUtf8("بِسْم"), QStringLiteral("kasra_mark"));
    QVERIFY2(!kasraBasePng.isEmpty() && !kasraMarkPng.isEmpty(), "add-text/render must succeed for the kasra probe");

    const QImage kasraBase(kasraBasePng);
    const QImage kasraMarked(kasraMarkPng);
    QVERIFY2(!kasraBase.isNull() && !kasraMarked.isNull(), "kasra probe PNGs must load");

    const QVector<int> kasraBaseRows = inkRows(kasraBase);
    QVERIFY2(!kasraBaseRows.isEmpty(), "kasra base render must contain ink");
    const int kasraBaseBottom = kasraBaseRows.last();

    const QVector<int> kasraMarkRows = diffRows(kasraBase, kasraMarked);
    QVERIFY2(!kasraMarkRows.isEmpty(), "the kasra mark must produce visible ink difference vs the plain base text");

    // (c) The kasra must render BELOW the base text (below the baseline).
    // Base "بسم" spans rows 75..91 (baseline ~92) in the original calibration.
    // Measured with the P6 glyph fix (2026-08-05): base bottom ~98..99, and
    // with a -3.93 Ts rise the kasra ink (which hangs BELOW its glyph origin,
    // ink box y -505..-236 font units) lands at rows ~99..102. The band is
    // base bottom + 2 — the kasra must visibly clear the deepest base letter.
    const int belowBandTop = kasraBaseBottom + 2;
    bool kasraInkBelow = false;
    for (int y : kasraMarkRows)
    {
        if (y >= belowBandTop)
        {
            kasraInkBelow = true;
            break;
        }
    }
    QVERIFY2(
        kasraInkBelow,
        qPrintable(QStringLiteral("P3: kasra must render BELOW the base text (ink at rows >= %1, base bottom row %2); "
                                  "today it never drops below the baseline (diff ink rows %3..%4)")
                       .arg(belowBandTop)
                       .arg(kasraBaseBottom)
                       .arg(kasraMarkRows.first())
                       .arg(kasraMarkRows.last())));
}

QTEST_GUILESS_MAIN(RtlAddTextTest)

#include "tst_rtladdtexttest.moc"
