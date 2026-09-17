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

// Regression test for R#4: /ActualText marked content must survive a page
// content-stream rewrite by the content editor.
//
// The RTL engine wraps every RTL run in
//   /Span << /ActualText <FEFF...> >> BDC ... EMC
// so extraction (PDFTextLayoutGenerator) can recover full ligatures the
// ToUnicode CMap degrades to one UTF-16 unit (lam-alef: 'لا' -> 'ل').
// The LTR add-text path (pdftooladdtext.cpp) rebuilds the page stream
// through PDFPageContentEditorProcessor + PDFPageContentEditorContentStreamBuilder,
// which historically dropped the marked content. This suite drives the exact
// CLI recipe of the degradation: RTL add-text (lam-alef), a second LTR
// add-text on the same page, then fetch-text.

#include "testsupport/tst_toolrunner.h"

#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

using testsupport::runAlbdfTool;
using testsupport::ToolResult;

class ActualTextTest : public QObject
{
    Q_OBJECT

private slots:
    void test_lamAlefSurvivesContentReEdit();
};

void ActualTextTest::test_lamAlefSurvivesContentReEdit()
{
    // R#4: RTL text added by a previous add-text --rtl must keep its
    // /ActualText overlay when a second (LTR) add-text rewrites the same
    // page's content stream. Logical "علا" shapes to a lam-alef ligature
    // glyph + ع; extraction must yield "لاع" (the full ligature at its
    // visual position), NOT the degraded "لع".
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString rtlPath = tmpDir.path() + QStringLiteral("/rtl.pdf");
    ToolResult addRtlResult = runAlbdfTool(toolPath,
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
                                            QString::fromUtf8("علا"),
                                            QStringLiteral("--size"),
                                            QStringLiteral("24"),
                                            QStringLiteral("--rtl"),
                                            QStringLiteral("--font"),
                                            QString::fromUtf8(TEST_FONT_PERSIAN),
                                            QStringLiteral("--lang"),
                                            QStringLiteral("fa")},
                                           tmpDir.path());
    QCOMPARE(addRtlResult.exitCode, 0);
    QVERIFY2(QFile::exists(rtlPath), "RTL output document must be created");

    // The RTL-only page must already extract the full ligature (P1 baseline).
    ToolResult fetchRtlResult = runAlbdfTool(toolPath, {QStringLiteral("fetch-text"), rtlPath}, tmpDir.path());
    QCOMPARE(fetchRtlResult.exitCode, 0);
    QVERIFY2(QString::fromUtf8(fetchRtlResult.stdoutData).contains(QString::fromUtf8("لاع")),
             "RTL-only page must extract the full lam-alef ligature");

    // Second add-text (LTR path): rewrites the page content stream through
    // PDFPageContentEditorProcessor + PDFPageContentEditorContentStreamBuilder.
    const QString editedPath = tmpDir.path() + QStringLiteral("/edited.pdf");
    ToolResult addLtrResult = runAlbdfTool(toolPath,
                                           {QStringLiteral("add-text"),
                                            rtlPath,
                                            editedPath,
                                            QStringLiteral("--page"),
                                            QStringLiteral("1"),
                                            QStringLiteral("--x"),
                                            QStringLiteral("72"),
                                            QStringLiteral("--y"),
                                            QStringLiteral("600"),
                                            QStringLiteral("--text"),
                                            QStringLiteral("Hello"),
                                            QStringLiteral("--size"),
                                            QStringLiteral("12")},
                                           tmpDir.path());
    QCOMPARE(addLtrResult.exitCode, 0);
    QVERIFY2(QFile::exists(editedPath), "re-edited output document must be created");

    ToolResult fetchEditedResult = runAlbdfTool(toolPath, {QStringLiteral("fetch-text"), editedPath}, tmpDir.path());
    QCOMPARE(fetchEditedResult.exitCode, 0);
    const QString fetched = QString::fromUtf8(fetchEditedResult.stdoutData);
    QVERIFY2(fetched.contains(QString::fromUtf8("لاع")),
             "extraction must keep the full lam-alef ligature after a content re-edit (R#4)");
    QVERIFY2(!fetched.contains(QString::fromUtf8("لع")),
             "extraction must NOT degrade the lam-alef ligature to its first letter after a content re-edit");
    QVERIFY2(fetched.contains(QString::fromUtf8("Hello")),
             "the LTR text added by the second add-text must still be present");
}

QTEST_GUILESS_MAIN(ActualTextTest)

#include "tst_actualtexttest.moc"
