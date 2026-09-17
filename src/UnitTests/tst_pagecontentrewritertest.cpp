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

// Unit/integration tests for pdf::PDFPageContentRewriter (R3.1), the shared
// write-back bridge for add-text (LTR/RTL) and delete-object.
//
// The image-doc fixture is the interesting case: page 1 has an INDIRECT
// /Resources (6 0 R) holding both a font (/F1) and a non-font resource
// (/XObject /Im1), and an indirect /Contents (5 0 R). That exercises the
// merge() trap where a referenced /Resources would otherwise be replaced.
//
// Covered:
//   - Append keeps the original content and grows /Contents (stream -> array,
//     array -> longer array) while preserving existing non-font resources.
//   - Replace installs the new bytes as the single content stream and still
//     preserves existing non-font resources.
//   - Two identical rewrites are byte-deterministic.
//   - The RTL CLI path (the real Append caller) preserves the XObject and is
//     byte-deterministic across runs.

#include "testsupport/tst_toolrunner.h"

#include "pdfcatalog.h"
#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfobject.h"
#include "pdfpage.h"
#include "pdfpagecontentrewriter.h"

#include <QtTest>

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>

#include <functional>

using testsupport::runAlbdfTool;
using testsupport::ToolResult;

namespace
{

bool readPdf(const QString& path, pdf::PDFDocument& document)
{
    pdf::PDFDocumentReader reader(nullptr, std::function<QString(bool*)>(), true, false);
    document = reader.readFromFile(path);
    return reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK;
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

const pdf::PDFDictionary* pageDictionary(const pdf::PDFDocument& document)
{
    const pdf::PDFPage* page = document.getCatalog()->getPage(0);
    if (!page)
    {
        return nullptr;
    }
    return document.getObjectByReference(page->getPageReference()).getDictionary();
}

const pdf::PDFObject& resolve(const pdf::PDFDocument& document, const pdf::PDFObject& object)
{
    if (object.isReference())
    {
        return document.getObjectByReference(object.getReference());
    }
    return object;
}

/// Concatenates the decoded bytes of every stream reachable through /Contents.
QByteArray decodedContent(const pdf::PDFDocument& document)
{
    const pdf::PDFDictionary* page = pageDictionary(document);
    if (!page)
    {
        return QByteArray();
    }

    const pdf::PDFObject& contentsObject = resolve(document, page->get("Contents"));
    pdf::PDFDocumentBuilder builder(&document);
    QByteArray result;

    const auto appendStream = [&](const pdf::PDFObject& object) {
        const pdf::PDFObject& streamObject = resolve(document, object);
        if (const pdf::PDFStream* stream = streamObject.getStream())
        {
            result += builder.getDecodedStream(stream);
        }
    };

    if (contentsObject.isArray())
    {
        const pdf::PDFArray* array = contentsObject.getArray();
        for (size_t i = 0; i < array->getCount(); ++i)
        {
            appendStream(array->getItem(i));
        }
    }
    else
    {
        appendStream(contentsObject);
    }

    return result;
}

pdf::PDFDictionary makeFontDictionary(const QByteArray& fontKey)
{
    pdf::PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem(fontKey);
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << pdf::PDFObject::createName("Font");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << pdf::PDFObject::createName("Type1");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BaseFont");
    factory << pdf::PDFObject::createName("Helvetica");
    factory.endDictionaryItem();
    factory.endDictionary();
    factory.endDictionaryItem();
    factory.endDictionary();

    const pdf::PDFObject object = factory.takeObject();
    const pdf::PDFDictionary* dictionary = object.getDictionary();
    return dictionary ? *dictionary : pdf::PDFDictionary();
}

pdf::PDFPageContentRewriter::Settings makeSettings(const pdf::PDFDocument& document,
                                                   pdf::PDFPageContentRewriter::ContentsMode mode,
                                                   const QString& outputPath)
{
    const pdf::PDFPage* page = document.getCatalog()->getPage(0);
    pdf::PDFPageContentRewriter::Settings settings;
    settings.pageReference = page->getPageReference();
    settings.fontDictionary = makeFontDictionary("F9");
    settings.contentBytes = QByteArrayLiteral("BT /F9 12 Tf 72 600 Td (AppendedFragment) Tj ET");
    settings.mode = mode;
    settings.outputPath = outputPath;
    return settings;
}

void verifyExistingResourcesPreserved(const pdf::PDFDocument& document)
{
    const pdf::PDFDictionary* page = pageDictionary(document);
    QVERIFY(page);
    const pdf::PDFObject& resources = resolve(document, page->get("Resources"));
    QVERIFY2(resources.isDictionary(), "page /Resources must resolve to a dictionary");
    const pdf::PDFDictionary* resourcesDictionary = resources.getDictionary();

    const pdf::PDFObject& fonts = resolve(document, resourcesDictionary->get("Font"));
    QVERIFY2(fonts.isDictionary(), "page /Font must resolve to a dictionary");
    QVERIFY2(fonts.getDictionary()->hasKey("F1"), "existing font F1 must be preserved");
    QVERIFY2(fonts.getDictionary()->hasKey("F9"), "new font F9 must be installed");

    const pdf::PDFObject& xobjects = resolve(document, resourcesDictionary->get("XObject"));
    QVERIFY2(xobjects.isDictionary(), "existing /XObject must be preserved");
    QVERIFY2(xobjects.getDictionary()->hasKey("Im1"), "existing image XObject Im1 must be preserved");
}

} // namespace

class PageContentRewriterTest : public QObject
{
    Q_OBJECT

private slots:
    void test_appendSingleStreamKeepsOriginal();
    void test_appendToArrayKeepsOriginal();
    void test_replaceReplacesContents();
    void test_determinism();
    void test_cliRtlAppendPreservesResources();
};

void PageContentRewriterTest::test_appendSingleStreamKeepsOriginal()
{
    pdf::PDFDocument document;
    QVERIFY2(readPdf(QString::fromUtf8(TEST_IMAGE_DOC_PDF), document), "cannot read image-doc fixture");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString outputPath = tmpDir.path() + QStringLiteral("/append-single.pdf");

    const pdf::PDFPageContentRewriter::Result result = pdf::PDFPageContentRewriter::rewrite(
        document, makeSettings(document, pdf::PDFPageContentRewriter::ContentsMode::Append, outputPath));
    QVERIFY2(result.isSuccess(), qPrintable(result.errorMessage));

    pdf::PDFDocument output;
    QVERIFY(readPdf(outputPath, output));

    const pdf::PDFDictionary* page = pageDictionary(output);
    QVERIFY(page);
    const pdf::PDFObject& contents = resolve(output, page->get("Contents"));
    QVERIFY2(contents.isArray(), "appending to a single stream must yield a /Contents array");
    QVERIFY2(contents.getArray()->getCount() == 2, "array must hold the original stream plus the new one");

    const QByteArray decoded = decodedContent(output);
    QVERIFY2(decoded.contains("EMBEDDED IMAGE DOCUMENT"), "original content must survive Append");
    QVERIFY2(decoded.contains("AppendedFragment"), "appended content must be present");

    verifyExistingResourcesPreserved(output);
}

void PageContentRewriterTest::test_appendToArrayKeepsOriginal()
{
    pdf::PDFDocument document;
    QVERIFY2(readPdf(QString::fromUtf8(TEST_IMAGE_DOC_PDF), document), "cannot read image-doc fixture");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString firstPath = tmpDir.path() + QStringLiteral("/append-array-1.pdf");
    const QString secondPath = tmpDir.path() + QStringLiteral("/append-array-2.pdf");

    const pdf::PDFPageContentRewriter::Result first = pdf::PDFPageContentRewriter::rewrite(
        document, makeSettings(document, pdf::PDFPageContentRewriter::ContentsMode::Append, firstPath));
    QVERIFY2(first.isSuccess(), qPrintable(first.errorMessage));

    pdf::PDFDocument intermediate;
    QVERIFY(readPdf(firstPath, intermediate));
    pdf::PDFPageContentRewriter::Settings secondSettings =
        makeSettings(intermediate, pdf::PDFPageContentRewriter::ContentsMode::Append, secondPath);
    secondSettings.contentBytes = QByteArrayLiteral("BT /F9 10 Tf 72 560 Td (SecondFragment) Tj ET");
    const pdf::PDFPageContentRewriter::Result second =
        pdf::PDFPageContentRewriter::rewrite(intermediate, secondSettings);
    QVERIFY2(second.isSuccess(), qPrintable(second.errorMessage));

    pdf::PDFDocument output;
    QVERIFY(readPdf(secondPath, output));

    const pdf::PDFDictionary* page = pageDictionary(output);
    QVERIFY(page);
    const pdf::PDFObject& contents = resolve(output, page->get("Contents"));
    QVERIFY2(contents.isArray(), "appending to an array must keep it an array");
    QVERIFY2(contents.getArray()->getCount() == 3, "array must grow by one entry per Append");

    const QByteArray decoded = decodedContent(output);
    QVERIFY2(decoded.contains("EMBEDDED IMAGE DOCUMENT"), "original content must survive chained Appends");
    QVERIFY2(decoded.contains("AppendedFragment"), "first appended content must survive");
    QVERIFY2(decoded.contains("SecondFragment"), "second appended content must be present");

    verifyExistingResourcesPreserved(output);
}

void PageContentRewriterTest::test_replaceReplacesContents()
{
    pdf::PDFDocument document;
    QVERIFY2(readPdf(QString::fromUtf8(TEST_IMAGE_DOC_PDF), document), "cannot read image-doc fixture");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString outputPath = tmpDir.path() + QStringLiteral("/replace.pdf");
    const QByteArray replacement = QByteArrayLiteral("BT /F9 12 Tf 72 600 Td (ReplacementFragment) Tj ET");

    pdf::PDFPageContentRewriter::Settings settings =
        makeSettings(document, pdf::PDFPageContentRewriter::ContentsMode::Replace, outputPath);
    settings.contentBytes = replacement;
    const pdf::PDFPageContentRewriter::Result result = pdf::PDFPageContentRewriter::rewrite(document, settings);
    QVERIFY2(result.isSuccess(), qPrintable(result.errorMessage));

    pdf::PDFDocument output;
    QVERIFY(readPdf(outputPath, output));

    const pdf::PDFDictionary* page = pageDictionary(output);
    QVERIFY(page);
    const pdf::PDFObject& contents = resolve(output, page->get("Contents"));
    QVERIFY2(contents.isStream(), "Replace must leave a single content stream");
    QCOMPARE(decodedContent(output), replacement);

    verifyExistingResourcesPreserved(output);
}

void PageContentRewriterTest::test_determinism()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString firstPath = tmpDir.path() + QStringLiteral("/det-1.pdf");
    const QString secondPath = tmpDir.path() + QStringLiteral("/det-2.pdf");

    for (const QString& outputPath : {firstPath, secondPath})
    {
        pdf::PDFDocument document;
        QVERIFY(readPdf(QString::fromUtf8(TEST_IMAGE_DOC_PDF), document));
        const pdf::PDFPageContentRewriter::Result result = pdf::PDFPageContentRewriter::rewrite(
            document, makeSettings(document, pdf::PDFPageContentRewriter::ContentsMode::Append, outputPath));
        QVERIFY2(result.isSuccess(), qPrintable(result.errorMessage));
    }

    QCOMPARE(sha256OfFile(firstPath), sha256OfFile(secondPath));
}

void PageContentRewriterTest::test_cliRtlAppendPreservesResources()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/albdf");
    QVERIFY2(QFile::exists(toolPath), qPrintable(QStringLiteral("albdf binary missing: %1").arg(toolPath)));

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString firstPath = tmpDir.path() + QStringLiteral("/rtl-1.pdf");
    const QString secondPath = tmpDir.path() + QStringLiteral("/rtl-2.pdf");

    const QStringList commonArguments = {QStringLiteral("add-text"),
                                         QString::fromUtf8(TEST_IMAGE_DOC_PDF),
                                         QString(),
                                         QStringLiteral("--page"),
                                         QStringLiteral("1"),
                                         QStringLiteral("--x"),
                                         QStringLiteral("72"),
                                         QStringLiteral("--y"),
                                         QStringLiteral("200"),
                                         QStringLiteral("--text"),
                                         QStringLiteral("سلام"),
                                         QStringLiteral("--rtl"),
                                         QStringLiteral("--font"),
                                         QString::fromUtf8(TEST_FONT_PERSIAN),
                                         QStringLiteral("--lang"),
                                         QStringLiteral("fa")};

    QStringList firstArguments = commonArguments;
    firstArguments[2] = firstPath;
    const ToolResult first = runAlbdfTool(toolPath, firstArguments, tmpDir.path());
    QCOMPARE(first.exitCode, 0);

    QStringList secondArguments = commonArguments;
    secondArguments[2] = secondPath;
    const ToolResult second = runAlbdfTool(toolPath, secondArguments, tmpDir.path());
    QCOMPARE(second.exitCode, 0);

    QCOMPARE(sha256OfFile(firstPath), sha256OfFile(secondPath));

    pdf::PDFDocument output;
    QVERIFY(readPdf(firstPath, output));

    const pdf::PDFDictionary* page = pageDictionary(output);
    QVERIFY(page);
    const pdf::PDFObject& resources = resolve(output, page->get("Resources"));
    QVERIFY2(resources.isDictionary(), "RTL Append must keep a resolvable /Resources");
    const pdf::PDFObject& xobjects = resolve(output, resources.getDictionary()->get("XObject"));
    QVERIFY2(xobjects.isDictionary() && xobjects.getDictionary()->hasKey("Im1"),
             "RTL Append must preserve the existing non-font XObject resource");
}

QTEST_GUILESS_MAIN(PageContentRewriterTest)

#include "tst_pagecontentrewritertest.moc"
