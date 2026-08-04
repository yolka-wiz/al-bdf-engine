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

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class SearchTextTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void test_hebrewSearch();
    void test_persianDigits();
    void test_zwnjInsensitive();
    void test_tashkeelInsensitive();
    void test_mixedBidi();
    void test_noFalsePositive();

private:
    struct ToolResult
    {
        int exitCode = -1;
        QByteArray stdoutData;
    };

    ToolResult runTool(const QString& toolPath, const QStringList& arguments, const QString& workDir) const;

    QString m_blankPdf;
    QString m_persianFont;
    QString m_arabicFont;
    QString m_hebrewFont;
};

void SearchTextTest::initTestCase()
{
    m_blankPdf = QString::fromUtf8(TEST_BLANK_PDF);
    m_persianFont = QString::fromUtf8(TEST_FONT_PERSIAN);
    m_arabicFont = QString::fromUtf8(TEST_FONT_ARABIC);
    m_hebrewFont = QString::fromUtf8(TEST_FONT_HEBREW);
}

SearchTextTest::ToolResult SearchTextTest::runTool(const QString& toolPath, const QStringList& arguments, const QString& workDir) const
{
    QProcess process;
    process.setWorkingDirectory(workDir);
    process.start(toolPath, arguments);
    if (!process.waitForStarted())
    {
        ToolResult failed;
        failed.exitCode = -100;
        return failed;
    }
    if (!process.waitForFinished(60000))
    {
        ToolResult failed;
        failed.exitCode = -101;
        return failed;
    }
    ToolResult result;
    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    return result;
}

void SearchTextTest::test_hebrewSearch()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("שלום עולם"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_hebrewFont,
        QStringLiteral("--lang"), QStringLiteral("he")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        QString::fromUtf8("שלום")
    }, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "Hebrew word must be found (count=1)");
}

void SearchTextTest::test_persianDigits()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/digits.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("۱۲۳"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_persianFont,
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    // Western digits must match Persian digits via digit unification.
    ToolResult searchResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        QStringLiteral("123")
    }, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "western '123' must find Persian '۱۲۳'");
}

void SearchTextTest::test_zwnjInsensitive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // میخواهم with ZWNJ between خ and و.
    const QString withZwnj = QString::fromUtf8("می\u200Cخواهم");
    const QString withoutZwnj = QString::fromUtf8("میخواهم");

    const QString inputPath = tmpDir.path() + QStringLiteral("/zwnj.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), withZwnj,
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_persianFont,
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        withoutZwnj
    }, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "query without ZWNJ must find text with ZWNJ");
}

void SearchTextTest::test_tashkeelInsensitive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // مُحَمَّد (with diacritics).
    const QString withTashkeel = QString::fromUtf8("مُحَمَّد");
    const QString withoutTashkeel = QString::fromUtf8("محمد");

    const QString inputPath = tmpDir.path() + QStringLiteral("/tashkeel.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), withTashkeel,
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_arabicFont,
        QStringLiteral("--lang"), QStringLiteral("ar")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        withoutTashkeel
    }, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "query without tashkeel must find text with tashkeel");
}

void SearchTextTest::test_mixedBidi()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/mixed.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("Hello سلام دنیا"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_persianFont,
        QStringLiteral("--lang"), QStringLiteral("fa")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult ltrResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        QStringLiteral("Hello")
    }, tmpDir.path());
    QCOMPARE(ltrResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(ltrResult.stdoutData).contains(QStringLiteral("1\n")),
             "LTR part of mixed text must be found");

    ToolResult rtlResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        QString::fromUtf8("دنیا")
    }, tmpDir.path());
    QCOMPARE(rtlResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(rtlResult.stdoutData).contains(QStringLiteral("1\n")),
             "RTL part of mixed text must be found");
}

void SearchTextTest::test_noFalsePositive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath, {
        QStringLiteral("add-text"),
        m_blankPdf,
        inputPath,
        QStringLiteral("--page"), QStringLiteral("1"),
        QStringLiteral("--x"), QStringLiteral("72"),
        QStringLiteral("--y"), QStringLiteral("700"),
        QStringLiteral("--text"), QString::fromUtf8("שלום עולם"),
        QStringLiteral("--size"), QStringLiteral("24"),
        QStringLiteral("--rtl"),
        QStringLiteral("--font"), m_hebrewFont,
        QStringLiteral("--lang"), QStringLiteral("he")
    }, tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    // "world" in English must NOT match Hebrew.
    ToolResult searchResult = runTool(toolPath, {
        QStringLiteral("search-text"),
        inputPath,
        QStringLiteral("world")
    }, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("0\n")),
             "non-matching query must report 0 matches");
}

QTEST_GUILESS_MAIN(SearchTextTest)

#include "tst_searchtexttest.moc"
