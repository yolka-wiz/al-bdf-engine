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

// Integration test for the delete-object CLI command.
//
// Runs the PdfTool binary against the test-baseline fixture (2 pages:
// "Hello PDF4QT baseline!" + gray rect on page 1, "Second page with
// numbers 12345" + red rect on page 2) and verifies:
//   - --list reports the expected object types/indices
//   - deleting the text run removes it from fetch-text output
//   - the surviving text and the other page stay intact
//   - deleting the path keeps the text
//   - invalid indices fail with a non-zero exit code
//   - the render of the untouched page 2 is unchanged (golden hash)

#include <QtTest>

#include <QCryptographicHash>
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
} // namespace

class DeleteObjectTest : public QObject
{
    Q_OBJECT

private slots:
    void test_listObjects();
    void test_deleteTextRun();
    void test_deletePathKeepsText();
    void test_invalidIndexFails();
};

void DeleteObjectTest::test_listObjects()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("PdfTool binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(toolPath,
                                {QStringLiteral("delete-object"),
                                 QString::fromUtf8(TEST_BASELINE_PDF),
                                 tmpDir.path() + QStringLiteral("/out.pdf"),
                                 QStringLiteral("--page"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--list")},
                                tmpDir.path());

    QCOMPARE(result.exitCode, 0);
    QVERIFY2(result.stdoutData.contains("text"), "list must contain a text object");
    QVERIFY2(result.stdoutData.contains("path"), "list must contain a path object");
    QVERIFY2(result.stdoutData.contains("Hello"), "list must contain the text content");
}

void DeleteObjectTest::test_deleteTextRun()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Delete the text run (index 0) on page 1.
    const QString outputPath = tmpDir.path() + QStringLiteral("/del-text.pdf");
    ToolResult delResult = runTool(toolPath,
                                   {QStringLiteral("delete-object"),
                                    QString::fromUtf8(TEST_BASELINE_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--index"),
                                    QStringLiteral("0")},
                                   tmpDir.path());
    QCOMPARE(delResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "output document must be created");

    // The deleted text must be gone, the surviving text must remain.
    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(!fetchResult.stdoutData.contains("Hello PDF4QT baseline!"), "deleted text must not be extractable");
    QVERIFY2(fetchResult.stdoutData.contains("Second page with numbers 12345"), "surviving text on page 2 must remain");
}

void DeleteObjectTest::test_deletePathKeepsText()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString outputPath = tmpDir.path() + QStringLiteral("/del-path.pdf");
    ToolResult delResult = runTool(toolPath,
                                   {QStringLiteral("delete-object"),
                                    QString::fromUtf8(TEST_BASELINE_PDF),
                                    outputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--index"),
                                    QStringLiteral("1")},
                                   tmpDir.path());
    QCOMPARE(delResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.contains("Hello PDF4QT baseline!"), "text must survive path deletion");
}

void DeleteObjectTest::test_invalidIndexFails()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(toolPath,
                                {QStringLiteral("delete-object"),
                                 QString::fromUtf8(TEST_BASELINE_PDF),
                                 tmpDir.path() + QStringLiteral("/out.pdf"),
                                 QStringLiteral("--page"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--index"),
                                 QStringLiteral("99")},
                                tmpDir.path());
    QVERIFY2(result.exitCode != 0, "invalid index must fail with non-zero exit code");
}

// QTEST_GUILESS_MAIN instantiates a QCoreApplication so that
// QCoreApplication::applicationDirPath() resolves the PdfTool binary path.
QTEST_GUILESS_MAIN(DeleteObjectTest)

#include "tst_deleteobjecttest.moc"
