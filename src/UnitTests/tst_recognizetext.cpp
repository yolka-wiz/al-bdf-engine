// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (c) 2026 pdfedit contributors
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
// This file is part of the pdfedit project, a fork of PDF4QT (MIT).
// The upstream PDF4QT portions remain under the MIT License; see the
// upstream copyright headers and the LICENSE file.

#include <QtTest>

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocument.h"
#include "pdfdocumentreader.h"
#include "pdffont.h"
#include "pdfmeshqualitysettings.h"
#include "pdfoptionalcontent.h"
#include "pdfrecognizetext.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

/// Tests for the recognize-text page object recognition (index/bbox/text/charBoxes
/// contract used by the recognize-text CLI command and consumed by delete-object).
class RecognizeTextTest : public QObject
{
    Q_OBJECT

private slots:
    void test_page1_text_object_json();
    void test_page2_objects();
};

void RecognizeTextTest::test_page1_text_object_json()
{
    pdf::PDFDocument document;
    pdf::PDFDocumentReader reader(
        nullptr,
        [](bool* ok) {
            *ok = true;
            return QString();
        },
        true,
        false);
    document = reader.readFromFile(TEST_BASELINE_PDF);
    QVERIFY2(document.getCatalog() != nullptr, "Failed to load the test-baseline.pdf fixture");
    QCOMPARE(document.getCatalog()->getPageCount(), 2);

    pdf::PDFOptionalContentActivity optionalContentActivity(&document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(&document);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();
    pdf::PDFMeshQualitySettings meshQualitySettings;
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFModifiedDocument md(&document, &optionalContentActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFRecognizeText recognizer(&document, &fontCache, cms.get(), &optionalContentActivity, &meshQualitySettings);

    std::vector<pdf::PDFRecognizeText::ObjectInfo> objects;
    for (pdf::PDFInteger page = 0; page < document.getCatalog()->getPageCount(); ++page)
    {
        std::vector<pdf::PDFRecognizeText::ObjectInfo> pageObjects = recognizer.recognize(page);
        objects.insert(objects.end(), pageObjects.begin(), pageObjects.end());
    }

    // The generated JSON must parse and be structured per the spec.
    QString json = pdf::PDFRecognizeText::toJson(objects);
    QJsonParseError parseError;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(jsonDocument.isObject());
    QVERIFY(jsonDocument.object().contains("pages"));
    QCOMPARE(jsonDocument.object().value("pages").toArray().size(), 2);

    // Page 1: the first content element is the text object (index 0, 0-based,
    // same index space as the delete-object command).
    QVERIFY2(objects.size() >= 4, qPrintable(QString("expected at least 4 objects, got %1").arg(objects.size())));
    const pdf::PDFRecognizeText::ObjectInfo& first = objects.front();
    QCOMPARE(first.page, 1);
    QCOMPARE(first.index, 0);
    QCOMPARE(first.type, QStringLiteral("text"));
    QVERIFY2(first.text.contains(QStringLiteral("Hello")), qPrintable(first.text));
    QVERIFY2(first.bbox.width() > 0.0 && first.bbox.height() > 0.0, "text object must have a nonzero bounding box");
    QVERIFY(first.fontSize > 0.0);
    QVERIFY(!first.font.isEmpty());
    QVERIFY2(first.charBoxes.size() >= 5,
             qPrintable(QString("expected at least 5 character boxes, got %1").arg(first.charBoxes.size())));
    for (const QRectF& charBox : first.charBoxes)
    {
        QVERIFY(charBox.width() > 0.0);
        QVERIFY(charBox.height() > 0.0);
    }

    // Page 1: the second content element is the gray rectangle (path).
    QCOMPARE(objects[1].page, 1);
    QCOMPARE(objects[1].index, 1);
    QCOMPARE(objects[1].type, QStringLiteral("path"));
}

void RecognizeTextTest::test_page2_objects()
{
    pdf::PDFDocument document;
    pdf::PDFDocumentReader reader(
        nullptr,
        [](bool* ok) {
            *ok = true;
            return QString();
        },
        true,
        false);
    document = reader.readFromFile(TEST_BASELINE_PDF);
    QVERIFY2(document.getCatalog() != nullptr, "Failed to load the test-baseline.pdf fixture");

    pdf::PDFOptionalContentActivity optionalContentActivity(&document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(&document);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();
    pdf::PDFMeshQualitySettings meshQualitySettings;
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFModifiedDocument md(&document, &optionalContentActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFRecognizeText recognizer(&document, &fontCache, cms.get(), &optionalContentActivity, &meshQualitySettings);
    std::vector<pdf::PDFRecognizeText::ObjectInfo> objects = recognizer.recognize(1);

    QCOMPARE(objects.size(), size_t(2));
    QCOMPARE(objects[0].page, 2);
    QCOMPARE(objects[0].index, 0);
    QCOMPARE(objects[0].type, QStringLiteral("text"));
    QVERIFY(objects[0].text.contains(QStringLiteral("Second page")));
    QVERIFY(objects[0].bbox.width() > 0.0);
    QCOMPARE(objects[1].type, QStringLiteral("path"));
}

QTEST_MAIN(RecognizeTextTest)

#include "tst_recognizetext.moc"
