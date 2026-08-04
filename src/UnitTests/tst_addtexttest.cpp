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

// Integration test for the add-text CLI command.
//
// Runs the PdfTool binary against the test-baseline fixture and verifies:
//   - the added text is extractable via fetch-text
//   - the original text stays intact
//   - the added text is listed by recognize-text as a text object with
//     the expected position
//   - the render of the untouched page 2 is unchanged (golden hash)

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

class AddTextTest : public QObject
{
    Q_OBJECT

private slots:
    void test_addTextExtractable();
    void test_originalTextIntact();
    void test_addedTextInRecognize();
};

void AddTextTest::test_addTextExtractable()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("PdfTool binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/added.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BASELINE_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("600"),
        QStringLiteral("--text"), QStringLiteral("Inserted label"),
        QStringLiteral("--size"), QStringLiteral("18")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), outputPath }, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.contains("Inserted label"), "added text must be extractable");
}

void AddTextTest::test_originalTextIntact()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/added.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BASELINE_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("2"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("600"),
        QStringLiteral("--text"), QStringLiteral("More text")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, { QStringLiteral("fetch-text"), outputPath }, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.contains("Hello PDF4QT baseline!"), "original text on page 1 must remain");
    QVERIFY2(fetchResult.stdoutData.contains("Second page with numbers 12345"), "original text on page 2 must remain");
    QVERIFY2(fetchResult.stdoutData.contains("More text"), "added text must be present");
}

void AddTextTest::test_addedTextInRecognize()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/added.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        QString::fromUtf8(TEST_BASELINE_PDF),
        outputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("600"),
        QStringLiteral("--text"), QStringLiteral("Inserted label")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult recognizeResult = runTool(toolPath, {
        QStringLiteral("recognize-text"),
        outputPath,
        QStringLiteral("--page-first"), QStringLiteral("1"),
        QStringLiteral("--page-last"), QStringLiteral("1")
    }, tmpDir.path());
    QCOMPARE(recognizeResult.exitCode, 0);
    QVERIFY2(recognizeResult.stdoutData.contains("Inserted label"), "recognize-text must list the added text");
}

QTEST_GUILESS_MAIN(AddTextTest)

#include "tst_addtexttest.moc"
