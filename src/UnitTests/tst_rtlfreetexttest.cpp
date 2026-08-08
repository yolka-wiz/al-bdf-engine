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

// M14 regression: a FreeText annotation with RTL contents must produce an
// appearance stream that embeds a shaped font (Type0 + FontFile2), not the
// base-14 Helvetica QPdfWriter emits (which renders Arabic as tofu).
//
// The RTL branch lives in PDFDocumentBuilder::updateAnnotationAppearanceStreams
// (a QPainter cannot embed a Type0/FontFile2 dict into the AP /Resources), so
// this test drives the core builder directly: create a FreeText annotation
// with Arabic contents, build, write, reopen, and walk /AP /N -> /Resources
// /Font looking for a Type0 entry whose FontDescriptor has a FontFile2.

#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfpage.h"

using namespace pdf;

class UnitTestsRtlFreeText : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void test_freetextRtlAppearanceEmbedsFont();
};

void UnitTestsRtlFreeText::test_freetextRtlAppearanceEmbedsFont()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString inputPath = tmpDir.filePath(QStringLiteral("in.pdf"));
    const QString outputPath = tmpDir.filePath(QStringLiteral("out.pdf"));

    QFile fixture(TEST_BLANK_PDF);
    QVERIFY2(fixture.open(QIODevice::ReadOnly), "Cannot open TEST_BLANK_PDF");
    QFile inputFile(inputPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    inputFile.write(fixture.readAll());
    inputFile.close();

    PDFDocumentReader reader(nullptr, [](bool* ok) { *ok = false; return QString(); }, false, false);
    PDFDocument document = reader.readFromFile(inputPath);
    QVERIFY2(document.getCatalog() != nullptr, "Failed to load blank.pdf");
    const PDFPage* page = document.getCatalog()->getPage(0);
    QVERIFY(page != nullptr);
    const PDFObjectReference pageReference = page->getPageReference();

    PDFDocumentModifier modifier(&document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    QVERIFY(builder != nullptr);

    const QRectF rect(50.0, 700.0, 200.0, 40.0);
    PDFObjectReference annotationRef;
    try
    {
        annotationRef = builder->createAnnotationFreeText(
            pageReference, rect, QStringLiteral("title"), QStringLiteral("subj"),
            QString::fromUtf8("\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85"), // "سلام"
            TextAlignment(Qt::AlignLeft | Qt::AlignTop));
    }
    catch (const std::exception& e)
    {
        QFAIL(qPrintable(QStringLiteral("createAnnotationFreeText threw: %1").arg(QLatin1String(e.what()))));
    }
    QVERIFY(annotationRef.isValid());

    bool finalized = false;
    try
    {
        finalized = modifier.finalize();
    }
    catch (const std::exception& e)
    {
        QFAIL(qPrintable(QStringLiteral("finalize threw: %1").arg(QLatin1String(e.what()))));
    }
    QVERIFY2(finalized, "finalize failed");
    PDFDocumentWriter writer(nullptr);
    QVERIFY2(static_cast<bool>(writer.write(outputPath, modifier.getDocument().data(), false)), "write failed");

    // Reopen and walk the annotation AP.
    PDFDocument reopened = reader.readFromFile(outputPath);
    QVERIFY2(reopened.getCatalog() != nullptr, "Failed to reopen output");
    const PDFPage* reopenedPage = reopened.getCatalog()->getPage(0);
    QVERIFY(reopenedPage != nullptr);

    bool foundType0WithFontFile2 = false;
    const std::vector<PDFObjectReference> annotations = reopenedPage->getAnnotations();
    QVERIFY2(!annotations.empty(), "No annotations on page");
    for (const PDFObjectReference& annotationRef2 : annotations)
    {
        PDFAnnotationPtr annotation = PDFAnnotation::parse(&reopened.getStorage(), annotationRef2);
        QVERIFY(annotation != nullptr);

        // Walk /AP /N -> form stream -> /Resources /Font.
        const PDFObject normalObject = annotation->getAppearanceStreams().getAppearance(PDFAppeareanceStreams::Appearance::Normal);
        const PDFObject& formObject = reopened.getObject(normalObject);
        if (!formObject.isDictionary())
        {
            continue;
        }
        const PDFDictionary* formDict = formObject.getDictionary();
        const PDFObject& resourcesObject = reopened.getObject(formDict->get("Resources"));
        if (!resourcesObject.isDictionary())
        {
            continue;
        }
        const PDFDictionary* resourcesDict = resourcesObject.getDictionary();
        const PDFObject& fontsObject = reopened.getObject(resourcesDict->get("Font"));
        if (!fontsObject.isDictionary())
        {
            continue;
        }
        const PDFDictionary* fontsDict = fontsObject.getDictionary();
        for (size_t i = 0; i < fontsDict->getCount(); ++i)
        {
            const PDFObject& fontEntry = reopened.getObject(fontsDict->getValue(i));
            if (!fontEntry.isDictionary())
            {
                continue;
            }
            const PDFDictionary* fontDict = fontEntry.getDictionary();
            const PDFObject& subtypeObject = reopened.getObject(fontDict->get("Subtype"));
            if (subtypeObject.getString() != "Type0")
            {
                continue;
            }
            const PDFObject& descriptorObject = reopened.getObject(fontDict->get("FontDescriptor"));
            if (!descriptorObject.isDictionary())
            {
                continue;
            }
            const PDFDictionary* descriptorDict = descriptorObject.getDictionary();
            if (!descriptorDict->get("FontFile2").isNull())
            {
                foundType0WithFontFile2 = true;
                break;
            }
        }
        if (foundType0WithFontFile2)
        {
            break;
        }
    }

    QVERIFY2(foundType0WithFontFile2,
             "FreeText RTL AP does not embed a Type0 font with FontFile2 (tofu)");
}

// The FreeText creation path consults QFontDatabase, so a GUI app instance
// is required (QTEST_GUILESS_MAIN aborts on QFontDatabase access).
QTEST_MAIN(UnitTestsRtlFreeText)
#include "tst_rtlfreetexttest.moc"
