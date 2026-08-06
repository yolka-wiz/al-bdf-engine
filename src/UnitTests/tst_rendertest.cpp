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

// Regression tests for fuzz-found `albdf render` argument validation bugs
// (DB #32 F#1, DB #33 F#2):
//   - --page-first 0  (or --page-last 999999999) crashed with SIGABRT
//     (uncaught std::out_of_range from a Qt Concurrent worker thread)
//     instead of failing with the documented invalid-arguments exit code
//   - --image-res-dpi 999999 never completed (multi-gigapixel image,
//     resource exhaustion) instead of being rejected up front
//
// All three abusive invocations must now terminate cleanly with the
// documented error code 7 (ErrorInvalidArguments), never crash with a
// signal and never hang. A valid render must keep working and stay
// byte-deterministic across runs.

#include <QtTest>

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace
{
struct ToolResult
{
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    bool finishedInTime = false;
    QByteArray stdoutData;
    QByteArray stderrData;
};

ToolResult
runTool(const QString& toolPath, const QStringList& arguments, const QString& workingDir, int timeoutMs = 180000)
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
    result.finishedInTime = process.waitForFinished(timeoutMs);
    if (!result.finishedInTime)
    {
        process.kill();
        process.waitForFinished(5000);
        return result;
    }
    result.exitStatus = process.exitStatus();
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

class RenderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderPageFirstZeroFailsWithErrorCode();
    void renderPageLastHugeFailsWithErrorCode();
    void renderHugeDpiDoesNotHang();
    void renderValidSinglePagePositiveControl();
    void renderOutputIsByteDeterministic();

private:
    QString m_toolPath;
    QString m_pdfPath;
    QString m_workingDir;
};

void RenderTest::initTestCase()
{
    m_toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(m_toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(m_toolPath)));
    m_pdfPath = QString::fromUtf8(TEST_MULTIPAGE_PDF);
    QVERIFY2(QFile::exists(m_pdfPath), qPrintable(QStringLiteral("multipage fixture missing: %1").arg(m_pdfPath)));
    m_workingDir = QFileInfo(m_pdfPath).absolutePath();
}

void RenderTest::renderPageFirstZeroFailsWithErrorCode()
{
    // F#1: page index 0 must be rejected with the documented error code (7),
    // not crash with SIGABRT (uncaught std::out_of_range from a worker thread).
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(m_toolPath,
                                {QStringLiteral("render"),
                                 m_pdfPath,
                                 QStringLiteral("--page-first"),
                                 QStringLiteral("0"),
                                 QStringLiteral("--page-last"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--image-format"),
                                 QStringLiteral("png"),
                                 QStringLiteral("--image-res-dpi"),
                                 QStringLiteral("72"),
                                 QStringLiteral("--image-output-dir"),
                                 tmpDir.path()},
                                m_workingDir);

    QVERIFY2(result.finishedInTime, "render must terminate, not hang");
    QCOMPARE(result.exitStatus, QProcess::NormalExit);
    QCOMPARE(result.exitCode, 7);
}

void RenderTest::renderPageLastHugeFailsWithErrorCode()
{
    // F#1: a page beyond the document page count must be rejected with the
    // documented error code (7), not crash with SIGABRT.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(m_toolPath,
                                {QStringLiteral("render"),
                                 m_pdfPath,
                                 QStringLiteral("--page-first"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--page-last"),
                                 QStringLiteral("999999999"),
                                 QStringLiteral("--image-format"),
                                 QStringLiteral("png"),
                                 QStringLiteral("--image-res-dpi"),
                                 QStringLiteral("72"),
                                 QStringLiteral("--image-output-dir"),
                                 tmpDir.path()},
                                m_workingDir);

    QVERIFY2(result.finishedInTime, "render must terminate, not hang");
    QCOMPARE(result.exitStatus, QProcess::NormalExit);
    QCOMPARE(result.exitCode, 7);
}

void RenderTest::renderHugeDpiDoesNotHang()
{
    // F#2: an absurd --image-res-dpi (999999) must be rejected up front with
    // the documented error code (7) and must never attempt to allocate a
    // multi-gigapixel image. The process must complete within a sane bound.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(m_toolPath,
                                {QStringLiteral("render"),
                                 m_pdfPath,
                                 QStringLiteral("--page-first"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--page-last"),
                                 QStringLiteral("1"),
                                 QStringLiteral("--image-format"),
                                 QStringLiteral("png"),
                                 QStringLiteral("--image-res-dpi"),
                                 QStringLiteral("999999"),
                                 QStringLiteral("--image-output-dir"),
                                 tmpDir.path()},
                                m_workingDir,
                                60000);

    QVERIFY2(result.finishedInTime, "render with huge dpi must terminate within 60 s, not hang");
    QCOMPARE(result.exitStatus, QProcess::NormalExit);
    QCOMPARE(result.exitCode, 7);
}

void RenderTest::renderValidSinglePagePositiveControl()
{
    // Positive control: a valid render of a single page at 72 dpi must
    // succeed (exit 0) and produce a non-empty PNG.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ToolResult result = runTool(m_toolPath,
                                {QStringLiteral("render"),
                                 m_pdfPath,
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
                                m_workingDir);

    QCOMPARE(result.exitStatus, QProcess::NormalExit);
    QCOMPARE(result.exitCode, 0);

    const QString imagePath = tmpDir.path() + QStringLiteral("/Image_1.png");
    QVERIFY2(QFile::exists(imagePath), "rendered page image must exist");
    QVERIFY2(QFileInfo(imagePath).size() > 0, "rendered page image must not be empty");
}

void RenderTest::renderOutputIsByteDeterministic()
{
    // Determinism rule: the same render must produce byte-identical output
    // across two runs.
    QTemporaryDir dirA;
    QTemporaryDir dirB;
    QVERIFY(dirA.isValid());
    QVERIFY(dirB.isValid());

    const QStringList common = {QStringLiteral("render"),
                                m_pdfPath,
                                QStringLiteral("--page-first"),
                                QStringLiteral("1"),
                                QStringLiteral("--page-last"),
                                QStringLiteral("1"),
                                QStringLiteral("--image-format"),
                                QStringLiteral("png"),
                                QStringLiteral("--image-res-dpi"),
                                QStringLiteral("72")};

    QStringList argsA = common;
    argsA << QStringLiteral("--image-output-dir") << dirA.path();
    QStringList argsB = common;
    argsB << QStringLiteral("--image-output-dir") << dirB.path();

    ToolResult resultA = runTool(m_toolPath, argsA, m_workingDir);
    ToolResult resultB = runTool(m_toolPath, argsB, m_workingDir);
    QCOMPARE(resultA.exitStatus, QProcess::NormalExit);
    QCOMPARE(resultA.exitCode, 0);
    QCOMPARE(resultB.exitStatus, QProcess::NormalExit);
    QCOMPARE(resultB.exitCode, 0);

    const QByteArray hashA = sha256OfFile(dirA.path() + QStringLiteral("/Image_1.png"));
    const QByteArray hashB = sha256OfFile(dirB.path() + QStringLiteral("/Image_1.png"));
    QVERIFY2(!hashA.isEmpty() && !hashB.isEmpty(), "both rendered images must exist");
    QCOMPARE(hashA, hashB);
}

QTEST_GUILESS_MAIN(RenderTest)

#include "tst_rendertest.moc"
