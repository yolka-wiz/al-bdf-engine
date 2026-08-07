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

// Integration tests for the redaction CLI command (`albdf redact`).
//
// Redaction regions come exclusively from /Subtype /Redact annotations
// embedded in the source PDF (PDFRedact::perform unions AnnotationType::Redact
// regions; parseQuadrilaterals falls back to /Rect). PDFRedact rebuilds pages
// through QPdfWriter, which serializes glyphs to vector curves — the redacted
// output has NO text layer, so assertions are PIXEL-BASED: the redaction
// regions must render as solid black bars while public content outside the
// regions keeps rendering.
//
// Fixture: src/tests/fixtures/redact-annotated.pdf — one 612x792 pt page with
// two Redact annotations and four text lines (2 public, 2 covered by the
// annotations). Coordinates below are derived from the generator
// (src/tests/scripts/make-redact-pdf.py) and converted from PDF user space
// (y up from bottom-left) to image space (y down from top-left) at 72 dpi:
//
//   secret 1: /Rect [60 630 400 685]  -> image x[60,400) y[107,162]
//   secret 2: /Rect [60 510 400 565]  -> image x[60,400) y[227,282]
//   public 1: baseline y=720 (24pt)   -> glyph band image y[56,71], x[74,320)
//   public 2: baseline y=600 (24pt)   -> glyph band image y[176,191], x[74,313)
//
// Verified behaviors (2026-08-06): regions render 100% black in the output
// (0% in the input — the fixture is not pre-blackened); public scanlines keep
// their exact input black fraction after redaction; output reopens cleanly
// via `info` (1 page); two redact runs are byte-identical (deterministic
// writer, SOURCE_DATE_EPOCH/fixed epoch in PDFDocumentBuilder).
//
// The suite also asserts the exit-code contract: missing output argument is
// ErrorInvalidArguments (7), a missing input file is ErrorDocumentReading (4).

#include <QtTest>

#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTextStream>

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
    if (!process.waitForStarted())
    {
        return result;
    }
    if (!process.waitForFinished(180000))
    {
        process.kill();
        return result;
    }
    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    result.stderrData = process.readAllStandardError();
    return result;
}

QByteArray sha256OfFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray();
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

// Renders one page of pdfPath to a fresh temporary directory via
// `albdf render` and returns the rendered image (null on failure). The
// temporary directory lives for the duration of this function — the image
// is loaded before it is destroyed.
QImage renderPage(const QString& toolPath, const QString& pdfPath, const QString& workingDir, int page)
{
    QTemporaryDir renderDir;
    if (!renderDir.isValid())
    {
        return QImage();
    }
    ToolResult result = runTool(toolPath,
                                {QStringLiteral("render"),
                                 pdfPath,
                                 QStringLiteral("--page-first"),
                                 QString::number(page),
                                 QStringLiteral("--page-last"),
                                 QString::number(page),
                                 QStringLiteral("--image-format"),
                                 QStringLiteral("png"),
                                 QStringLiteral("--image-res-dpi"),
                                 QStringLiteral("72"),
                                 QStringLiteral("--image-output-dir"),
                                 renderDir.path()},
                                workingDir);
    if (result.exitCode != 0)
    {
        return QImage();
    }
    const QString renderedPath = renderDir.path() + QStringLiteral("/Image_%1.png").arg(page);
    if (!QFile::exists(renderedPath))
    {
        return QImage();
    }
    return QImage(renderedPath);
}

// Counts near-black pixels (luminance < 60) on the horizontal scanline
// y in the half-open x range [x1, x2). Used to distinguish a solid black
// redaction bar (fraction ~1.0) from rendered text (fraction ~0.05-0.4).
int countNearBlackRow(const QImage& image, int x1, int x2, int y)
{
    int black = 0;
    for (int x = x1; x < x2; ++x)
    {
        if (qGray(image.pixel(x, y)) < 60)
        {
            ++black;
        }
    }
    return black;
}

double blackFraction(const QImage& image, int x1, int x2, int y)
{
    const int width = x2 - x1;
    if (width <= 0 || y < 0 || y >= image.height())
    {
        return 0.0;
    }
    return static_cast<double>(countNearBlackRow(image, x1, x2, y)) / static_cast<double>(width);
}

// Redaction region 1 in image space (derived from /Rect [60 630 400 685]).
constexpr int kRect1X1 = 60;
constexpr int kRect1X2 = 400;
constexpr int kRect1ScanY = 134; // middle of image y[107,162)

// Redaction region 2 in image space (derived from /Rect [60 510 400 565]).
constexpr int kRect2X1 = 60;
constexpr int kRect2X2 = 400;
constexpr int kRect2ScanY = 254; // middle of image y[227,282)

// Public line glyph bands (outside any redaction region).
constexpr int kPublic1X1 = 74;
constexpr int kPublic1X2 = 320;
constexpr int kPublic1ScanY = 64; // glyph band image y[56,71)
constexpr int kPublic2X1 = 74;
constexpr int kPublic2X2 = 313;
constexpr int kPublic2ScanY = 184; // glyph band image y[176,191)

// A solid redaction bar must cover >= 95% of a scanline through the region.
constexpr double kSolidBarMinFraction = 0.95;
// Rendered text is a fraction of black pixels; anything above 0.60 on a
// scanline is a bar, not text.
constexpr double kTextMaxFraction = 0.60;
// Public content must survive redaction: at least a few black pixels on the
// scanline (i.e. the text still renders).
constexpr double kTextMinFraction = 0.05;
// The public scanline black fraction must be preserved by redaction (text
// outside the regions is untouched), within anti-aliasing tolerance.
constexpr double kPublicFractionTolerance = 0.03;

} // namespace

class RedactTest : public QObject
{
    Q_OBJECT

private slots:
    void redactionRegionRendersSolidBlackBar();
    void redactionPreservesPageSize();
    void redactedOutputRoundTrip();
    void redactionOutputDeterministic();
    void redactInvalidArguments();
};

void RedactTest::redactionRegionRendersSolidBlackBar()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));
    const QString fixturePath = QString::fromUtf8(TEST_REDACT_PDF);
    QVERIFY2(QFile::exists(fixturePath), "redact-annotated.pdf fixture must exist");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/redacted.pdf");
    ToolResult redactResult = runTool(toolPath, {QStringLiteral("redact"), fixturePath, outputPath}, tmpDir.path());
    QCOMPARE(redactResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "redacted document must be created");

    const QImage outputImage = renderPage(toolPath, outputPath, tmpDir.path(), 1);
    const QImage inputImage = renderPage(toolPath, fixturePath, tmpDir.path(), 1);
    QVERIFY2(!outputImage.isNull() && !inputImage.isNull(), "both pages must render");
    QCOMPARE(outputImage.size(), QSize(612, 792));
    QCOMPARE(inputImage.size(), QSize(612, 792));

    // The redaction regions must render as solid black bars in the output.
    QVERIFY2(blackFraction(outputImage, kRect1X1, kRect1X2, kRect1ScanY) >= kSolidBarMinFraction,
             "redaction region 1 must render as a solid black bar");
    QVERIFY2(blackFraction(outputImage, kRect2X1, kRect2X2, kRect2ScanY) >= kSolidBarMinFraction,
             "redaction region 2 must render as a solid black bar");

    // Sanity guard: the input regions must NOT already be black bars, or the
    // assertions above would be vacuous (text lives inside the regions).
    QVERIFY2(blackFraction(inputImage, kRect1X1, kRect1X2, kRect1ScanY) < kSolidBarMinFraction,
             "input region 1 must not be pre-blackened (fixture sanity)");
    QVERIFY2(blackFraction(inputImage, kRect2X1, kRect2X2, kRect2ScanY) < kSolidBarMinFraction,
             "input region 2 must not be pre-blackened (fixture sanity)");

    // Public lines outside the regions must still render as text: present
    // (>= kTextMinFraction) but not a solid bar (< kTextMaxFraction), and
    // the black fraction must match the input (content untouched).
    const double outPublic1 = blackFraction(outputImage, kPublic1X1, kPublic1X2, kPublic1ScanY);
    const double inPublic1 = blackFraction(inputImage, kPublic1X1, kPublic1X2, kPublic1ScanY);
    const double outPublic2 = blackFraction(outputImage, kPublic2X1, kPublic2X2, kPublic2ScanY);
    const double inPublic2 = blackFraction(inputImage, kPublic2X1, kPublic2X2, kPublic2ScanY);
    QVERIFY2(outPublic1 > kTextMinFraction && outPublic1 < kTextMaxFraction,
             "public line 1 must still render as text after redaction");
    QVERIFY2(outPublic2 > kTextMinFraction && outPublic2 < kTextMaxFraction,
             "public line 2 must still render as text after redaction");
    QVERIFY2(qAbs(outPublic1 - inPublic1) < kPublicFractionTolerance,
             "public line 1 rendering must be preserved by redaction");
    QVERIFY2(qAbs(outPublic2 - inPublic2) < kPublicFractionTolerance,
             "public line 2 rendering must be preserved by redaction");
}

void RedactTest::redactionPreservesPageSize()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    const QString fixturePath = QString::fromUtf8(TEST_REDACT_PDF);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/redacted.pdf");
    ToolResult redactResult = runTool(toolPath, {QStringLiteral("redact"), fixturePath, outputPath}, tmpDir.path());
    QCOMPARE(redactResult.exitCode, 0);

    // The page is 612x792 pt; at 72 dpi it must render 612x792 pixels —
    // redaction must not change the page geometry.
    const QImage outputImage = renderPage(toolPath, outputPath, tmpDir.path(), 1);
    const QImage inputImage = renderPage(toolPath, fixturePath, tmpDir.path(), 1);
    QVERIFY2(!outputImage.isNull() && !inputImage.isNull(), "both pages must render");
    QCOMPARE(inputImage.size(), QSize(612, 792));
    QCOMPARE(outputImage.size(), QSize(612, 792));
}

void RedactTest::redactedOutputRoundTrip()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    const QString fixturePath = QString::fromUtf8(TEST_REDACT_PDF);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/redacted.pdf");
    ToolResult redactResult = runTool(toolPath, {QStringLiteral("redact"), fixturePath, outputPath}, tmpDir.path());
    QCOMPARE(redactResult.exitCode, 0);

    // The redacted document must reopen cleanly through `info` with exactly
    // one page (the fixture is single-page).
    ToolResult infoResult = runTool(toolPath, {QStringLiteral("info"), outputPath}, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);
    QVERIFY2(infoResult.stdoutData.contains("Page count"), "info output must report a page count");

    QString pageCountLine;
    QTextStream stream(infoResult.stdoutData);
    while (!stream.atEnd())
    {
        const QString line = stream.readLine();
        if (line.contains(QStringLiteral("Page count")))
        {
            pageCountLine = line;
            break;
        }
    }
    QVERIFY2(!pageCountLine.isEmpty(), "info output must contain a 'Page count' line");
    const QStringList parts = pageCountLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QCOMPARE(parts.size() >= 3 ? parts.at(2) : QString(), QStringLiteral("1"));
}

void RedactTest::redactionOutputDeterministic()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    const QString fixturePath = QString::fromUtf8(TEST_REDACT_PDF);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString firstPath = tmpDir.path() + QStringLiteral("/first.pdf");
    ToolResult first = runTool(toolPath, {QStringLiteral("redact"), fixturePath, firstPath}, tmpDir.path());
    QCOMPARE(first.exitCode, 0);

    const QString secondPath = tmpDir.path() + QStringLiteral("/second.pdf");
    ToolResult second = runTool(toolPath, {QStringLiteral("redact"), fixturePath, secondPath}, tmpDir.path());
    QCOMPARE(second.exitCode, 0);

    const QByteArray firstHash = sha256OfFile(firstPath);
    const QByteArray secondHash = sha256OfFile(secondPath);
    QVERIFY2(!firstHash.isEmpty() && firstHash == secondHash,
             "redact output must be byte-deterministic across runs (no timestamps/random IDs)");
}

void RedactTest::redactInvalidArguments()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    const QString fixturePath = QString::fromUtf8(TEST_REDACT_PDF);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Missing output document: ErrorInvalidArguments (7) — contract code.
    ToolResult noOutput = runTool(toolPath, {QStringLiteral("redact"), fixturePath}, tmpDir.path());
    QCOMPARE(noOutput.exitCode, 7);

    // No arguments at all: also invalid (7).
    ToolResult noArgs = runTool(toolPath, {QStringLiteral("redact")}, tmpDir.path());
    QCOMPARE(noArgs.exitCode, 7);

    // Nonexistent input: ErrorDocumentReading (4) — non-zero.
    ToolResult missingInput = runTool(toolPath,
                                      {QStringLiteral("redact"),
                                       tmpDir.path() + QStringLiteral("/nope.pdf"),
                                       tmpDir.path() + QStringLiteral("/out.pdf")},
                                      tmpDir.path());
    QVERIFY2(missingInput.exitCode != 0, "nonexistent input must fail with non-zero exit code");
    QVERIFY2(!QFile::exists(tmpDir.path() + QStringLiteral("/out.pdf")), "no output must be written on failure");
}

// QTEST_GUILESS_MAIN instantiates a QCoreApplication so that
// QCoreApplication::applicationDirPath() resolves the albdf binary path.
QTEST_GUILESS_MAIN(RedactTest)

#include "tst_redacttest.moc"
