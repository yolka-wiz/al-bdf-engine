// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Integration test for the RTL add-text path (--rtl --font --lang).
//
// Runs PdfTool against the blank fixture with the bundled OFL fonts and verifies:
//   - RTL text is embedded as Type0/Identity-H (info-fonts lists it, embedded=Yes)
//   - Hebrew round-trips exactly through fetch-text (no ligatures/marks)
//   - Persian extracts with expected degradation (lam-alef ligature -> lam)
//   - the RTL font is registered under a separate key (F2) so LTR text (F1)
//     still works on the same page
//   - rendering the RTL page produces no render errors

#include <QtTest>

#include <QFile>
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
}   // namespace

class RtlAddTextTest : public QObject
{
    Q_OBJECT

private slots:
    void test_hebrewRoundTrip();
    void test_persianExtraction();
    void test_rtlFontEmbedded();
    void test_rtlKeepsLtrIntact();
    void test_rtlRenderNoErrors();
    void test_rtlNotMirrored();
};

void RtlAddTextTest::test_hebrewRoundTrip()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("PdfTool binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("שלום עולם"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_HEBREW),
        QStringLiteral("--lang"), QStringLiteral("he")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), outputPath }, tmpDir.path());
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
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("سلام دنیا"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_PERSIAN),
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), outputPath }, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    // Lam-alef ligature degrades to lam in ToUnicode (2-byte CMap limit).
    // RTL extracts in visual order: "سلام دنیا" -> reversed runs.
    // "سلام دنیا" logical -> visual: "ایند ملس" (with لا -> ل: "ایند ملس").
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("ایند ملس")),
             "Persian text must extract in visual order with lam-alef degraded");
}

void RtlAddTextTest::test_rtlFontEmbedded()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("سلام"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_PERSIAN),
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult infoResult = runTool(toolPath, { QStringLiteral("info-fonts"), outputPath }, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);
    QVERIFY2(infoResult.stdoutData.contains("Vazirmatn"), "RTL font must be embedded");
    QVERIFY2(infoResult.stdoutData.contains("Type 0"), "RTL font must be Type0/CID");
    QVERIFY2(infoResult.stdoutData.contains("Yes"), "RTL font must be marked embedded");
}

void RtlAddTextTest::test_rtlKeepsLtrIntact()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // RTL first, then LTR on the same page.
    const QString rtlPath = tmpDir.path() + QStringLiteral("/rtl.pdf");
    ToolResult rtlResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        rtlPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("سلام"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_PERSIAN),
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(rtlResult.exitCode, 0);

    const QString mixedPath = tmpDir.path() + QStringLiteral("/mixed.pdf");
    ToolResult ltrResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        rtlPath,
        mixedPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("600"),
        QStringLiteral("--text"), QStringLiteral("LTR still OK"),
        QStringLiteral("--size"), QStringLiteral("16")
    }, tmpDir.path());
    QCOMPARE(ltrResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), mixedPath }, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains("LTR still OK"), "LTR text must survive RTL add");
    // Lam-alef ligature degrades to lam in extraction (ToUnicode 2-byte limit).
    // RTL extracts in visual order: "سلام" -> "ملس" (لا ligature -> ل: "ملس").
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("ملس")),
             "RTL text must survive LTR add");
}

void RtlAddTextTest::test_rtlRenderNoErrors()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/fa.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("سلام دنیا"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_PERSIAN),
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult renderResult = runTool(toolPath, {
        QStringLiteral("render"),
        outputPath,
        QStringLiteral("--page-first"), QStringLiteral("1"),
        QStringLiteral("--page-last"), QStringLiteral("1"),
        QStringLiteral("--image-format"), QStringLiteral("png"),
        QStringLiteral("--image-res-dpi"), QStringLiteral("72"),
        QStringLiteral("--image-output-dir"), tmpDir.path()
    }, tmpDir.path());
    QCOMPARE(renderResult.exitCode, 0);
    // The "Rendering Errors" section header is always printed; check that no
    // actual error row follows it (an error row contains "Error" + a message).
    QVERIFY2(!renderResult.stdoutData.contains("FreeType"), "no FreeType errors in RTL render");
    QVERIFY2(!renderResult.stdoutData.contains(QByteArray("Error      ")),
             "RTL page must render without errors");
    QVERIFY2(QFile::exists(tmpDir.path() + QStringLiteral("/Image_1.png")),
             "render must produce an image");
}

void RtlAddTextTest::test_rtlNotMirrored()
{
    // Mirror-regression: with a correct content stream, RTL text extracts in
    // VISUAL order (entire line reversed). If someone reintroduces the fpdf2
    // #1802 glyph reversal, extraction returns LOGICAL order and this fails.
    // (Verified against Qt's bidi-aware QPainter reference: glyph positions
    // match only without the reversal — HarfBuzz >= 4 emits RTL runs
    // leftmost-first already.)
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BLANK_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("אבג"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), QString::fromUtf8(TEST_FONT_HEBREW),
        QStringLiteral("--lang"), QStringLiteral("he")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), outputPath }, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(fetchResult.stdoutData).contains(QString::fromUtf8("גבא")),
             "RTL must extract in visual order (אבג -> גבא); glyph reversal would mirror the render");
}

QTEST_GUILESS_MAIN(RtlAddTextTest)

#include "tst_rtladdtexttest.moc"
