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

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "pdfdocument.h"
#include "pdfdocumentreader.h"
#include "pdfdocumenttextflow.h"
#include "pdftextsearchengine.h"

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
    void test_crossItemPhrase();
    void test_crossItemNoFalsePositive();
    void test_crossLinePhrase();
    void test_presentationFormSearch();
    void test_engineSearchSalam();

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

SearchTextTest::ToolResult
SearchTextTest::runTool(const QString& toolPath, const QStringList& arguments, const QString& workDir) const
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
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
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
                                    m_hebrewFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("he")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, QString::fromUtf8("שלום")}, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "Hebrew word must be found (count=1)");
}

void SearchTextTest::test_persianDigits()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/digits.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("۱۲۳"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    m_persianFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    // Western digits must match Persian digits via digit unification.
    ToolResult searchResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, QStringLiteral("123")}, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "western '123' must find Persian '۱۲۳'");
}

void SearchTextTest::test_zwnjInsensitive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // میخواهم with ZWNJ between خ and و.
    const QString withZwnj = QString::fromUtf8("می\u200Cخواهم");
    const QString withoutZwnj = QString::fromUtf8("میخواهم");

    const QString inputPath = tmpDir.path() + QStringLiteral("/zwnj.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    withZwnj,
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    m_persianFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult = runTool(toolPath, {QStringLiteral("search-text"), inputPath, withoutZwnj}, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "query without ZWNJ must find text with ZWNJ");
}

void SearchTextTest::test_tashkeelInsensitive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // مُحَمَّد (with diacritics).
    const QString withTashkeel = QString::fromUtf8("مُحَمَّد");
    const QString withoutTashkeel = QString::fromUtf8("محمد");

    const QString inputPath = tmpDir.path() + QStringLiteral("/tashkeel.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    withTashkeel,
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    m_arabicFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("ar")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult searchResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, withoutTashkeel}, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("1\n")),
             "query without tashkeel must find text with tashkeel");
}

void SearchTextTest::test_mixedBidi()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/mixed.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
                                    QStringLiteral("--page"),
                                    QStringLiteral("1"),
                                    QStringLiteral("--x"),
                                    QStringLiteral("72"),
                                    QStringLiteral("--y"),
                                    QStringLiteral("700"),
                                    QStringLiteral("--text"),
                                    QString::fromUtf8("Hello سلام دنیا"),
                                    QStringLiteral("--size"),
                                    QStringLiteral("24"),
                                    QStringLiteral("--rtl"),
                                    QStringLiteral("--font"),
                                    m_persianFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("fa")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    ToolResult ltrResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, QStringLiteral("Hello")}, tmpDir.path());
    QCOMPARE(ltrResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(ltrResult.stdoutData).contains(QStringLiteral("1\n")),
             "LTR part of mixed text must be found");

    ToolResult rtlResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, QString::fromUtf8("دنیا")}, tmpDir.path());
    QCOMPARE(rtlResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(rtlResult.stdoutData).contains(QStringLiteral("1\n")),
             "RTL part of mixed text must be found");
}

void SearchTextTest::test_noFalsePositive()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/he.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
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
                                    m_hebrewFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("he")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    // "world" in English must NOT match Hebrew.
    ToolResult searchResult =
        runTool(toolPath, {QStringLiteral("search-text"), inputPath, QStringLiteral("world")}, tmpDir.path());
    QCOMPARE(searchResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(searchResult.stdoutData).contains(QStringLiteral("0\n")),
             "non-matching query must report 0 matches");
}

void SearchTextTest::test_crossItemPhrase()
{
    // S#1: a phrase split across two adjacent text-flow items must be found.
    // The CLI/add-text path cannot produce a deterministic 2-item flow (the
    // docstrum Layout algorithm merges adjacent runs on synthetic pages), so
    // drive the engine's searchFlow() test seam directly with a hand-built
    // flow: two RTL runs on the same line, word-sized gap (~11 pt at 24 pt).
    //
    // RTL visual order: "نهایی" renders LEFT (visual "ییاهن"), "تست" RIGHT.
    // Left-to-right concatenation must reconstruct "ییاهن تست", which is the
    // visual form of the logical query "تست نهایی".
    pdf::PDFDocumentTextFlow flow;

    pdf::PDFDocumentTextFlow::Item item1;
    item1.pageIndex = 0;
    item1.text = QString::fromUtf8("ییاهن");
    item1.boundingRect = QRectF(72.0, 700.0, 67.25, 17.53);
    item1.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item1);

    pdf::PDFDocumentTextFlow::Item item2;
    item2.pageIndex = 0;
    item2.text = QString::fromUtf8("تست");
    item2.boundingRect = QRectF(150.0, 700.0, 28.64, 17.06);
    item2.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item2);

    pdf::PDFTextSearchEngine engine;
    const auto matches =
        engine.searchFlow(flow, QString::fromUtf8("تست نهایی"), 0, 0, pdf::PDFTextSearchEngine::Options());
    QCOMPARE(matches.size(), size_t(1));
    QCOMPARE(matches.front().spans.size(), size_t(2));
    // The matched text must reconstruct the full phrase including the
    // word-space separator (this is what the CLI "Text" column shows).
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral(" ")),
             "matched text must contain the space separator");
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral("تست")), "matched text must contain the second word");
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral("اهن")), "matched text must contain the first word");
    // The bounding rect is the union across both spans (page coordinates).
    QVERIFY2(matches.front().boundingRect.left() <= 72.0 + 1.0, "bounding rect must start at the first item");
    QVERIFY2(matches.front().boundingRect.right() >= 150.0 + 28.64 - 1.0, "bounding rect must end at the second item");
}

void SearchTextTest::test_crossItemNoFalsePositive()
{
    // S#1 guard: far-apart items must NOT be joined into a false phrase match.
    // Two LTR runs on the same line, 288 pt apart (a column gap, not a word
    // space) — a naive space-join of every item would falsely match.
    pdf::PDFDocumentTextFlow flow;

    pdf::PDFDocumentTextFlow::Item item1;
    item1.pageIndex = 0;
    item1.text = QStringLiteral("hello");
    item1.boundingRect = QRectF(72.0, 700.0, 40.0, 17.0);
    item1.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item1);

    pdf::PDFDocumentTextFlow::Item item2;
    item2.pageIndex = 0;
    item2.text = QStringLiteral("world");
    item2.boundingRect = QRectF(400.0, 700.0, 50.0, 17.0);
    item2.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item2);

    pdf::PDFTextSearchEngine engine;
    const auto matches =
        engine.searchFlow(flow, QStringLiteral("hello world"), 0, 0, pdf::PDFTextSearchEngine::Options());
    QCOMPARE(matches.size(), size_t(0));
}

void SearchTextTest::test_crossLinePhrase()
{
    // S#1: a phrase with a space must match across the '\n' line boundary.
    // Same visual-string conventions as test_crossItemPhrase: the visual
    // form of "تست نهایی" is "ییاهن تست" (نهایی renders LEFT, تست RIGHT).
    // Here the phrase is split across TWO lines (different y): the first
    // line carries the visual-left part "ییاهن", the second line carries
    // "تست", with a small paragraph-like leading gap (6.5 pt vs ~17 pt line
    // height). Today the between-line separator is the hard '\n' boundary,
    // so the query's space can never cross it and the phrase cannot match.
    pdf::PDFDocumentTextFlow flow;

    pdf::PDFDocumentTextFlow::Item item1;
    item1.pageIndex = 0;
    item1.text = QString::fromUtf8("ییاهن"); // visual for نهایی (first line)
    item1.boundingRect = QRectF(72.0, 700.0, 67.25, 17.53);
    item1.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item1);

    pdf::PDFDocumentTextFlow::Item item2;
    item2.pageIndex = 0;
    item2.text = QString::fromUtf8("تست"); // second line, below item1
    item2.boundingRect = QRectF(150.0, 724.0, 28.64, 17.06);
    item2.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item2);

    pdf::PDFTextSearchEngine engine;
    const auto matches =
        engine.searchFlow(flow, QString::fromUtf8("تست نهایی"), 0, 0, pdf::PDFTextSearchEngine::Options());
    QCOMPARE(matches.size(), size_t(1));
    QCOMPARE(matches.front().spans.size(), size_t(2));
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral(" ")),
             "matched text must contain the between-line space separator");
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral("تست")), "matched text must contain the second word");
    QVERIFY2(matches.front().matchedText.contains(QStringLiteral("اهن")), "matched text must contain the first word");
    QVERIFY2(matches.front().boundingRect.top() <= 700.0 + 1.0, "bounding rect must start at the first line");
    QVERIFY2(matches.front().boundingRect.bottom() >= 724.0 + 17.06 - 1.0, "bounding rect must end at the second line");
}

void SearchTextTest::test_presentationFormSearch()
{
    // R#3 (DB #23): Arabic presentation-form shaping in fribidi_log2vis
    // requires a second NFKC pass after inversion in search. Without it,
    // Arabic search breaks. Pin the behavior: a query in base letters must
    // match a flow whose item text uses presentation forms (or vice versa)
    // — the NFKC pass is what folds the forms back to base.
    //
    // 'لام' logical (ل ا م) shapes to [لا ligature, م]; visual order left-to-
    // right is 'م' then the lam-alef ligature: "\u0645\uFEFB". The base-letter
    // query 'لام' (U+0644 U+0627 U+0645) must match it.
    pdf::PDFDocumentTextFlow flow;

    pdf::PDFDocumentTextFlow::Item item1;
    item1.pageIndex = 0;
    item1.text = QString::fromUtf8("\u0645\uFEFB"); // م + لا ligature (visual order)
    item1.boundingRect = QRectF(72.0, 700.0, 40.0, 17.0);
    item1.flags = pdf::PDFDocumentTextFlow::Text;
    flow.addItem(item1);

    pdf::PDFTextSearchEngine engine;
    const auto matches =
        engine.searchFlow(flow, QString::fromUtf8("\u0644\u0627\u0645"), 0, 0, pdf::PDFTextSearchEngine::Options());
    QVERIFY2(matches.size() >= 1, "presentation-form lam-alef must be found by a base-letter query (NFKC pass, R#3)");
}

void SearchTextTest::test_engineSearchSalam()
{
    // GUI search now routes plain-text queries through PDFTextSearchEngine
    // (pdfwidgetrtlsearch.h adapter). Pin the exact engine path the GUI uses:
    // a document containing the RTL fixture text 'سلام' (add-text --rtl,
    // Noto Naskh Arabic — same recipe as src/tests/fixtures/gui-rtl.pdf)
    // must be found by the logical-order query via the document-level
    // PDFTextSearchEngine::search API.
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString inputPath = tmpDir.path() + QStringLiteral("/salam.pdf");
    ToolResult addResult = runTool(toolPath,
                                   {QStringLiteral("add-text"),
                                    m_blankPdf,
                                    inputPath,
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
                                    m_arabicFont,
                                    QStringLiteral("--lang"),
                                    QStringLiteral("ar")},
                                   tmpDir.path());
    QCOMPARE(addResult.exitCode, 0);

    pdf::PDFDocument document;
    pdf::PDFDocumentReader reader(
        nullptr,
        [](bool* ok) {
            *ok = true;
            return QString();
        },
        true,
        false);
    document = reader.readFromFile(inputPath);
    QVERIFY2(document.getCatalog() != nullptr, "Failed to load the add-text output");

    pdf::PDFTextSearchEngine engine;
    const auto matches = engine.search(&document, QString::fromUtf8("سلام"), 0, 0);
    QCOMPARE(matches.size(), size_t(1));
    QVERIFY2(!matches.front().matchedText.isEmpty(), "match must carry the matched text");

    // Negative control: a near-miss query (wrong first letter) must not match.
    const auto misses = engine.search(&document, QString::fromUtf8("شلام"), 0, 0);
    QCOMPARE(misses.size(), size_t(0));
}

QTEST_GUILESS_MAIN(SearchTextTest)

#include "tst_searchtexttest.moc"
