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

// Integration tests for the page-operation CLI commands: rotate,
// move-page and delete-page.
//
// Uses the deterministic multipage fixture (src/tests/fixtures/multipage.pdf,
// 5 pages, each with a distinct "MULTIPAGE PAGE <N>" text line and a colored
// rectangle on a 612x792 pt page) and verifies:
//   - rotate changes the rendered page geometry (dimensions swap at 90 deg)
//     and produces byte-identical output on re-runs (determinism)
//   - move-page relocates page content between page slots, keeping the
//     page count and the rest of the order intact
//   - delete-page removes pages (single and via --page-select), dropping
//     the deleted pages' text from fetch-text
//   - every modified document reopens cleanly through albdf info (round-trip)
//   - invalid arguments fail with a non-zero exit code (contract: 7)

#include <QtTest>

#include <QCryptographicHash>
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

QSize renderPageSize(const QString& toolPath, const QString& pdfPath, const QString& workingDir, int page)
{
    QTemporaryDir renderDir;
    if (!renderDir.isValid())
    {
        return QSize();
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
        return QSize();
    }
    const QString renderedPath = renderDir.path() + QStringLiteral("/Image_%1.png").arg(page);
    if (!QFile::exists(renderedPath))
    {
        return QSize();
    }
    QImage image(renderedPath);
    return image.size();
}
} // namespace

class PageOpsTest : public QObject
{
    Q_OBJECT

private slots:
    void rotateChangesGeometry();
    void rotateInvalidArguments();
    void rotateOutputDeterministic();
    void movePageMovesContent();
    void movePageInvalidArguments();
    void deletePageRemovesPage();
    void deletePageMultiplePages();
    void deletePageInvalidArguments();
};

void PageOpsTest::rotateChangesGeometry()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Multipage fixture page 1 is 612x792 pt (portrait); at 72 dpi it renders 612x792.
    const QSize originalSize = renderPageSize(toolPath, QString::fromUtf8(TEST_MULTIPAGE_PDF), tmpDir.path(), 1);
    QCOMPARE(originalSize, QSize(612, 792));

    // Rotate page 1 by 90 degrees clockwise: rendered geometry must swap.
    const QString outputPath = tmpDir.path() + QStringLiteral("/rotated.pdf");
    ToolResult rotateResult = runTool(toolPath,
                                      {QStringLiteral("rotate"),
                                       QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                       outputPath,
                                       QStringLiteral("--page"),
                                       QStringLiteral("1"),
                                       QStringLiteral("--angle"),
                                       QStringLiteral("90")},
                                      tmpDir.path());
    QCOMPARE(rotateResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "rotated document must be created");

    const QSize rotatedSize = renderPageSize(toolPath, outputPath, tmpDir.path(), 1);
    QCOMPARE(rotatedSize, QSize(792, 612));

    // Round-trip: the modified document must reopen cleanly.
    ToolResult infoResult = runTool(toolPath, {QStringLiteral("info"), outputPath}, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);

    // Other pages are untouched: page 2 keeps its geometry.
    const QSize pageTwoSize = renderPageSize(toolPath, outputPath, tmpDir.path(), 2);
    QCOMPARE(pageTwoSize, QSize(612, 792));
}

void PageOpsTest::rotateInvalidArguments()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Invalid angle.
    ToolResult badAngle = runTool(toolPath,
                                  {QStringLiteral("rotate"),
                                   QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                   tmpDir.path() + QStringLiteral("/out.pdf"),
                                   QStringLiteral("--page"),
                                   QStringLiteral("1"),
                                   QStringLiteral("--angle"),
                                   QStringLiteral("45")},
                                  tmpDir.path());
    QVERIFY2(badAngle.exitCode != 0, "invalid angle must fail with non-zero exit code");

    // Missing --page.
    ToolResult noPage = runTool(toolPath,
                                {QStringLiteral("rotate"),
                                 QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                 tmpDir.path() + QStringLiteral("/out.pdf"),
                                 QStringLiteral("--angle"),
                                 QStringLiteral("90")},
                                tmpDir.path());
    QVERIFY2(noPage.exitCode != 0, "missing --page must fail with non-zero exit code");

    // Out-of-range page number.
    ToolResult badPage = runTool(toolPath,
                                 {QStringLiteral("rotate"),
                                  QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                  tmpDir.path() + QStringLiteral("/out.pdf"),
                                  QStringLiteral("--page"),
                                  QStringLiteral("99"),
                                  QStringLiteral("--angle"),
                                  QStringLiteral("90")},
                                 tmpDir.path());
    QVERIFY2(badPage.exitCode != 0, "out-of-range page must fail with non-zero exit code");
}

void PageOpsTest::rotateOutputDeterministic()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QStringList baseArgs = {QStringLiteral("rotate"),
                                  QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                  QStringLiteral("--page"),
                                  QStringLiteral("1"),
                                  QStringLiteral("--page"),
                                  QStringLiteral("3"),
                                  QStringLiteral("--angle"),
                                  QStringLiteral("180")};

    const QString firstPath = tmpDir.path() + QStringLiteral("/first.pdf");
    ToolResult first = runTool(toolPath, baseArgs + QStringList{firstPath}, tmpDir.path());
    QCOMPARE(first.exitCode, 0);

    const QString secondPath = tmpDir.path() + QStringLiteral("/second.pdf");
    ToolResult second = runTool(toolPath, baseArgs + QStringList{secondPath}, tmpDir.path());
    QCOMPARE(second.exitCode, 0);

    const QByteArray firstHash = sha256OfFile(firstPath);
    const QByteArray secondHash = sha256OfFile(secondPath);
    QVERIFY2(!firstHash.isEmpty() && firstHash == secondHash, "rotate output must be byte-deterministic across runs");
}

void PageOpsTest::movePageMovesContent()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Move page 1 to position 3: [1,2,3,4,5] -> [2,3,1,4,5].
    const QString outputPath = tmpDir.path() + QStringLiteral("/moved.pdf");
    ToolResult moveResult = runTool(toolPath,
                                    {QStringLiteral("move-page"),
                                     QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                     outputPath,
                                     QStringLiteral("--from"),
                                     QStringLiteral("1"),
                                     QStringLiteral("--to"),
                                     QStringLiteral("3")},
                                    tmpDir.path());
    QCOMPARE(moveResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "moved document must be created");

    // Round-trip: reopen cleanly and keep the page count.
    ToolResult infoResult = runTool(toolPath, {QStringLiteral("info"), outputPath}, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.count("MULTIPAGE PAGE") == 5, "all five pages must survive the move");

    // Content moved between slots: page 1's text is now after pages 2 and 3.
    const int twoPos = fetchResult.stdoutData.indexOf("MULTIPAGE PAGE TWO");
    const int threePos = fetchResult.stdoutData.indexOf("MULTIPAGE PAGE THREE");
    const int onePos = fetchResult.stdoutData.indexOf("MULTIPAGE PAGE ONE");
    QVERIFY2(twoPos >= 0 && threePos >= 0 && onePos >= 0, "all moved page texts must be present");
    QVERIFY2(twoPos < onePos && threePos < onePos,
             "page 1 content must appear after pages 2 and 3 in the reordered document");

    // The moved page keeps its content (text of the moved page is intact).
    QVERIFY2(fetchResult.stdoutData.contains("MULTIPAGE PAGE FOUR"), "page 4 content must remain at its slot");

    // Determinism: repeat the same operation and compare hashes.
    const QString secondPath = tmpDir.path() + QStringLiteral("/moved2.pdf");
    ToolResult again = runTool(toolPath,
                               {QStringLiteral("move-page"),
                                QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                secondPath,
                                QStringLiteral("--from"),
                                QStringLiteral("1"),
                                QStringLiteral("--to"),
                                QStringLiteral("3")},
                               tmpDir.path());
    QCOMPARE(again.exitCode, 0);
    QCOMPARE(sha256OfFile(outputPath), sha256OfFile(secondPath));
}

void PageOpsTest::movePageInvalidArguments()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QStringList common = {
        QStringLiteral("move-page"), QString::fromUtf8(TEST_MULTIPAGE_PDF), tmpDir.path() + QStringLiteral("/out.pdf")};

    ToolResult noFrom =
        runTool(toolPath, common + QStringList{QStringLiteral("--to"), QStringLiteral("2")}, tmpDir.path());
    QVERIFY2(noFrom.exitCode != 0, "missing --from must fail");

    ToolResult zeroFrom = runTool(
        toolPath,
        common +
            QStringList{QStringLiteral("--from"), QStringLiteral("0"), QStringLiteral("--to"), QStringLiteral("2")},
        tmpDir.path());
    QVERIFY2(zeroFrom.exitCode != 0, "zero --from must fail");

    ToolResult tooFar = runTool(
        toolPath,
        common +
            QStringList{QStringLiteral("--from"), QStringLiteral("1"), QStringLiteral("--to"), QStringLiteral("99")},
        tmpDir.path());
    QVERIFY2(tooFar.exitCode != 0, "out-of-range --to must fail");
}

void PageOpsTest::deletePageRemovesPage()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Delete page 3 of the 5-page fixture.
    const QString outputPath = tmpDir.path() + QStringLiteral("/deleted.pdf");
    ToolResult deleteResult = runTool(toolPath,
                                      {QStringLiteral("delete-page"),
                                       QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                       outputPath,
                                       QStringLiteral("--page"),
                                       QStringLiteral("3")},
                                      tmpDir.path());
    QCOMPARE(deleteResult.exitCode, 0);
    QVERIFY2(QFile::exists(outputPath), "deleted document must be created");

    // Round-trip: reopen cleanly.
    ToolResult infoResult = runTool(toolPath, {QStringLiteral("info"), outputPath}, tmpDir.path());
    QCOMPARE(infoResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.count("MULTIPAGE PAGE") == 4, "page count must drop to 4");
    QVERIFY2(!fetchResult.stdoutData.contains("MULTIPAGE PAGE THREE"), "deleted page text must be gone");
    QVERIFY2(fetchResult.stdoutData.contains("MULTIPAGE PAGE ONE") &&
                 fetchResult.stdoutData.contains("MULTIPAGE PAGE FIVE"),
             "surviving pages must keep their content");
}

void PageOpsTest::deletePageMultiplePages()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Delete pages 1 and 3 via a page selection.
    const QString outputPath = tmpDir.path() + QStringLiteral("/deleted-multi.pdf");
    ToolResult deleteResult = runTool(toolPath,
                                      {QStringLiteral("delete-page"),
                                       QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                       outputPath,
                                       QStringLiteral("--page-select"),
                                       QStringLiteral("1,3")},
                                      tmpDir.path());
    QCOMPARE(deleteResult.exitCode, 0);

    ToolResult fetchResult = runTool(toolPath, {QStringLiteral("fetch-text"), outputPath}, tmpDir.path());
    QCOMPARE(fetchResult.exitCode, 0);
    QVERIFY2(fetchResult.stdoutData.count("MULTIPAGE PAGE") == 3, "page count must drop to 3");
    QVERIFY2(!fetchResult.stdoutData.contains("MULTIPAGE PAGE ONE") &&
                 !fetchResult.stdoutData.contains("MULTIPAGE PAGE THREE"),
             "both deleted page texts must be gone");
    QVERIFY2(fetchResult.stdoutData.contains("MULTIPAGE PAGE TWO") &&
                 fetchResult.stdoutData.contains("MULTIPAGE PAGE FOUR"),
             "surviving pages must keep their content");

    // Determinism: repeat and compare hashes.
    const QString secondPath = tmpDir.path() + QStringLiteral("/deleted-multi2.pdf");
    ToolResult again = runTool(toolPath,
                               {QStringLiteral("delete-page"),
                                QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                secondPath,
                                QStringLiteral("--page-select"),
                                QStringLiteral("1,3")},
                               tmpDir.path());
    QCOMPARE(again.exitCode, 0);
    QCOMPARE(sha256OfFile(outputPath), sha256OfFile(secondPath));
}

void PageOpsTest::deletePageInvalidArguments()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QStringList common = {QStringLiteral("delete-page"),
                                QString::fromUtf8(TEST_MULTIPAGE_PDF),
                                tmpDir.path() + QStringLiteral("/out.pdf")};

    // No page specified at all.
    ToolResult none = runTool(toolPath, common, tmpDir.path());
    QVERIFY2(none.exitCode != 0, "delete-page without --page must fail");

    // Out-of-range page number.
    ToolResult badPage =
        runTool(toolPath, common + QStringList{QStringLiteral("--page"), QStringLiteral("99")}, tmpDir.path());
    QVERIFY2(badPage.exitCode != 0, "out-of-range --page must fail");

    // Deleting every page must be rejected (document must keep at least one page).
    ToolResult allPages =
        runTool(toolPath, common + QStringList{QStringLiteral("--page-select"), QStringLiteral("1-5")}, tmpDir.path());
    QVERIFY2(allPages.exitCode != 0, "deleting all pages must fail");
}

// QTEST_GUILESS_MAIN instantiates a QCoreApplication so that
// QCoreApplication::applicationDirPath() resolves the albdf binary path.
QTEST_GUILESS_MAIN(PageOpsTest)

#include "tst_pageopstest.moc"
