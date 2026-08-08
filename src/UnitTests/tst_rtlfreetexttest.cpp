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

#include <QDebug>
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
    QTemporaryDir keepDir;
    keepDir.setAutoRemove(false);
    const QString inputPath = tmpDir.filePath(QStringLiteral("in.pdf"));
    const QString outputPath = keepDir.path() + QStringLiteral("/out.pdf");

    QFile fixture(TEST_BLANK_PDF);
    QVERIFY2(fixture.open(QIODevice::ReadOnly), "Cannot open TEST_BLANK_PDF");
    QFile inputFile(inputPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    inputFile.write(fixture.readAll());
    inputFile.close();

    PDFDocumentReader reader(
        nullptr,
        [](bool* ok) {
            *ok = false;
            return QString();
        },
        false,
        false);
    PDFDocument document = reader.readFromFile(inputPath);
    qDebug() << "M1 loaded";
    QVERIFY2(document.getCatalog() != nullptr, "Failed to load blank.pdf");
    const PDFPage* page = document.getCatalog()->getPage(0);
    QVERIFY(page != nullptr);
    const PDFObjectReference pageReference = page->getPageReference();

    PDFDocumentModifier modifier(&document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    qDebug() << "M2 modifier+builder";
    QVERIFY(builder != nullptr);

    QFile fontFile(TEST_FONT_ARABIC);
    QVERIFY2(fontFile.open(QIODevice::ReadOnly), "Cannot open TEST_FONT_ARABIC");
    qDebug() << "M3 fontset";
    builder->setRtlFreeTextFontData(fontFile.readAll());

    const QRectF rect(50.0, 700.0, 200.0, 40.0);
    PDFObjectReference annotationRef;
    try
    {
        annotationRef =
            builder->createAnnotationFreeText(pageReference,
                                              rect,
                                              QStringLiteral("title"),
                                              QStringLiteral("subj"),
                                              QString::fromUtf8("\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85"), // "سلام"
                                              TextAlignment(Qt::AlignLeft | Qt::AlignTop));
    }
    catch (const std::exception& e)
    {
        QFAIL(qPrintable(QStringLiteral("createAnnotationFreeText threw: %1").arg(QLatin1String(e.what()))));
    }
    qDebug() << "M4 annotation created";
    QVERIFY(annotationRef.isValid());

    qDebug() << "TEST input pages:" << document.getCatalog()->getPageCount();
    qDebug() << "KEEP DIR:" << keepDir.path();
    {
        PDFDocumentWriter w0(nullptr);
        const QString rtPath = tmpDir.filePath(QStringLiteral("rt.pdf"));
        w0.write(rtPath, &document, false);
        PDFDocument rt = reader.readFromFile(rtPath);
        qDebug() << "ROUNDTRIP pages:" << (rt.getCatalog() ? rt.getCatalog()->getPageCount() : 999);
    }
    modifier.markAnnotationsChanged();
    bool finalized = false;
    try
    {
        finalized = modifier.finalize();
    }
    catch (const std::exception& e)
    {
        QFAIL(qPrintable(QStringLiteral("finalize threw: %1").arg(QLatin1String(e.what()))));
    }
    qDebug() << "M5 finalized";
    QVERIFY2(finalized, "finalize failed");
    PDFDocumentWriter writer(nullptr);
    bool written = false;
    try
    {
        written = static_cast<bool>(writer.write(outputPath, modifier.getDocument().data(), true));
    }
    catch (const std::exception& e)
    {
        QFAIL(qPrintable(QStringLiteral("writer.write threw: %1").arg(QLatin1String(e.what()))));
    }
    qDebug() << "M6 written";
    QVERIFY2(written, "write failed");

    // Reopen and walk the annotation AP.
    PDFDocument reopened;
    try
    {
        reopened = reader.readFromFile(outputPath);
    }
    catch (const std::exception& e)
    {
        qWarning() << "REOPEN THREW:" << e.what();
    }
    qDebug() << "M7 reopened";
    QVERIFY2(reopened.getCatalog() != nullptr, "Failed to reopen output");
    qDebug() << "M7b pages=" << reopened.getCatalog()->getPageCount();
    const PDFPage* reopenedPage = reopened.getCatalog()->getPage(0);
    QVERIFY(reopenedPage != nullptr);

    bool foundType0WithFontFile2 = false;
    try
    {
        qDebug() << "WALK: annotations on page";
        const std::vector<PDFObjectReference> annotations = reopenedPage->getAnnotations();
        qDebug() << "WALK: count" << annotations.size();
        QVERIFY2(!annotations.empty(), "No annotations on page");
        for (const PDFObjectReference& annotationRef2 : annotations)
        {
            PDFAnnotationPtr annotation = PDFAnnotation::parse(&reopened.getStorage(), annotationRef2);
            QVERIFY(annotation != nullptr);

            // Walk /AP /N -> form stream -> /Resources /Font.
            qDebug() << "WALK: parse ok";
            const PDFObject normalObject =
                annotation->getAppearanceStreams().getAppearance(PDFAppeareanceStreams::Appearance::Normal);
            qDebug() << "WALK: normal isNull" << normalObject.isNull() << "isRef" << normalObject.isReference()
                     << "isStream" << normalObject.isStream() << "isDict" << normalObject.isDictionary();
            const PDFObject& formObject = reopened.getObject(normalObject);
            // The AP /N is a Form XObject — a STREAM whose dictionary holds
            // /Resources. Accept stream or plain dictionary.
            const PDFDictionary* formDict = nullptr;
            if (formObject.isStream())
            {
                formDict = formObject.getStream()->getDictionary();
            }
            else if (formObject.isDictionary())
            {
                formDict = formObject.getDictionary();
            }
            qDebug() << "WALK: formDict" << (formDict != nullptr);
            if (!formDict)
            {
                continue;
            }
            const PDFObject& resourcesObject = reopened.getObject(formDict->get("Resources"));
            if (!resourcesObject.isDictionary())
            {
                continue;
            }
            const PDFDictionary* resourcesDict = resourcesObject.getDictionary();
            const PDFObject& fontsObject = reopened.getObject(resourcesDict->get("Font"));
            qDebug() << "WALK: fonts isDict" << fontsObject.isDictionary();
            if (!fontsObject.isDictionary())
            {
                continue;
            }
            const PDFDictionary* fontsDict = fontsObject.getDictionary();
            qDebug() << "WALK: font count" << fontsDict->getCount();
            for (size_t i = 0; i < fontsDict->getCount(); ++i)
            {
                const PDFObject& fontEntry = reopened.getObject(fontsDict->getValue(i));
                qDebug() << "WALK: fontEntry isDict" << fontEntry.isDictionary() << "isRef" << fontEntry.isReference()
                         << "isStream" << fontEntry.isStream();
                if (!fontEntry.isDictionary())
                {
                    continue;
                }
                const PDFDictionary* fontDict = fontEntry.getDictionary();
                const PDFObject& subtypeObject = reopened.getObject(fontDict->get("Subtype"));
                qDebug() << "WALK: subtype" << subtypeObject.getString();
                if (subtypeObject.getString() != "Type0")
                {
                    continue;
                }
                qDebug() << "WALK: PASSED Type0";
                // Standard Type0 layout: the FontDescriptor (with FontFile2)
                // lives in the descendant CIDFontType2 dict (DescendantFonts[0]),
                // not directly on the Type0 dict.
                const PDFObject& descendantFontsObject = reopened.getObject(fontDict->get("DescendantFonts"));
                if (!descendantFontsObject.isArray())
                {
                    continue;
                }
                const PDFArray* descendantFontsArray = descendantFontsObject.getArray();
                if (!descendantFontsArray || descendantFontsArray->getCount() == 0)
                {
                    continue;
                }
                const PDFObject& descendantObject = reopened.getObject(descendantFontsArray->getItem(0));
                if (!descendantObject.isDictionary())
                {
                    continue;
                }
                const PDFDictionary* descendantDict = descendantObject.getDictionary();
                const PDFObject& descriptorObject = reopened.getObject(descendantDict->get("FontDescriptor"));
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
    }
    catch (const std::exception& e)
    {
        qWarning() << "AP WALK THREW:" << e.what();
    }

    qDebug() << "M8 walked, found=" << foundType0WithFontFile2;
    QVERIFY2(foundType0WithFontFile2, "FreeText RTL AP does not embed a Type0 font with FontFile2 (tofu)");
}

// The FreeText creation path consults QFontDatabase, so a GUI app instance
// is required (QTEST_GUILESS_MAIN aborts on QFontDatabase access).
QTEST_MAIN(UnitTestsRtlFreeText)
#include "tst_rtlfreetexttest.moc"
