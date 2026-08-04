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

#include "pdftoolformlist.h"

#include "pdfcatalog.h"
#include "pdfdocument.h"
#include "pdfdocumentreader.h"
#include "pdfform.h"
#include "pdfoutputformatter.h"

namespace pdftool
{

static PDFToolFormList s_formListApplication;

QString PDFToolFormList::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "form-list";

    case Name:
        return PDFToolTranslationContext::tr("Form list");

    case Description:
        return PDFToolTranslationContext::tr("List interactive form fields (names, types, values, pages).");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

PDFToolAbstractApplication::Options PDFToolFormList::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument;
}

namespace
{

QString fieldTypeToString(const pdf::PDFFormField* field)
{
    switch (field->getFieldType())
    {
    case pdf::PDFFormField::FieldType::Button:
        return PDFToolTranslationContext::tr("button");

    case pdf::PDFFormField::FieldType::Text:
        return PDFToolTranslationContext::tr("text");

    case pdf::PDFFormField::FieldType::Choice:
        return PDFToolTranslationContext::tr("choice");

    case pdf::PDFFormField::FieldType::Signature:
        return PDFToolTranslationContext::tr("signature");

    default:
        return PDFToolTranslationContext::tr("invalid");
    }
}

/// Value of the field rendered as a string for the report.
QString fieldValueToString(const pdf::PDFFormField* field)
{
    const pdf::PDFObject& value = field->getValue();
    if (value.isString())
    {
        return value.getString();
    }
    if (value.isName())
    {
        return value.getString();
    }
    if (value.isBool())
    {
        return value.getBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isInt())
    {
        return QString::number(value.getInteger());
    }
    if (value.isReal())
    {
        return QString::number(value.getReal());
    }
    if (value.isNull())
    {
        return QString();
    }
    // Array (e.g. radio selection) or anything else: print the object type.
    return PDFToolTranslationContext::tr("(%1)").arg(int(value.getType()));
}

void listFieldRecursive(const pdf::PDFFormField* field,
                        const QString& qualifiedName,
                        PDFOutputFormatter& formatter,
                        const pdf::PDFDocument* document,
                        const pdf::PDFCatalog* catalog,
                        int depth)
{
    Q_UNUSED(depth);
    const QString name = field->getName(pdf::PDFFormField::FullyQualified).isEmpty()
                             ? qualifiedName
                             : field->getName(pdf::PDFFormField::FullyQualified);

    formatter.beginTableRow("field");
    formatter.writeTableColumn("name", name);
    formatter.writeTableColumn("type", fieldTypeToString(field));
    formatter.writeTableColumn("value", fieldValueToString(field));

    // Widget info: page + bounding rect of the first widget.
    const pdf::PDFFormWidgets& widgets = field->getWidgets();
    if (!widgets.empty())
    {
        const pdf::PDFFormWidget& widget = widgets.front();
        const pdf::PDFInteger pageIndex = catalog->getPageIndexFromPageReference(widget.getPage());
        QRectF rect;
        if (const pdf::PDFDictionary* widgetDictionary =
                document->getDictionaryFromObject(document->getObjectByReference(widget.getWidget())))
        {
            pdf::PDFDocumentDataLoaderDecorator loader(document);
            rect = loader.readRectangle(widgetDictionary->get("Rect"), QRectF());
        }
        formatter.writeTableColumn("page", QString::number(pageIndex + 1));
        formatter.writeTableColumn("rect",
                                   QStringLiteral("%1,%2,%3,%4")
                                       .arg(rect.left(), 0, 'f', 2)
                                       .arg(rect.top(), 0, 'f', 2)
                                       .arg(rect.width(), 0, 'f', 2)
                                       .arg(rect.height(), 0, 'f', 2));
    }
    else
    {
        formatter.writeTableColumn("page", QString());
        formatter.writeTableColumn("rect", QString());
    }

    const pdf::PDFFormField::FieldFlags fieldFlags = field->getFlags();
    formatter.writeTableColumn("readonly", (fieldFlags.testFlag(pdf::PDFFormField::ReadOnly)) ? "yes" : "no");
    formatter.endTableRow();

    // Children (grouped fields).
    for (const pdf::PDFFormFieldPointer& child : field->getChildFields())
    {
        listFieldRecursive(child.get(), name, formatter, document, catalog, depth + 1);
    }
}

} // namespace

int PDFToolFormList::execute(const PDFToolOptions& options)
{
    pdf::PDFDocument document;
    if (!readDocument(options, document, nullptr, false))
    {
        return ErrorDocumentReading;
    }

    const pdf::PDFObject formObject = document.getCatalog()->getFormObject();
    const pdf::PDFForm form = pdf::PDFForm::parse(&document, formObject);

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("form-fields", PDFToolTranslationContext::tr("Form fields of %1").arg(options.document));
    formatter.endl();

    if (form.getFormFields().empty())
    {
        formatter.writeText("form", PDFToolTranslationContext::tr("No form fields found in the document."));
        formatter.endl();
    }
    else
    {
        formatter.beginTable("fields", PDFToolTranslationContext::tr("Form fields"));
        formatter.beginTableHeaderRow("header");
        formatter.writeTableHeaderColumn("name", PDFToolTranslationContext::tr("Name"));
        formatter.writeTableHeaderColumn("type", PDFToolTranslationContext::tr("Type"));
        formatter.writeTableHeaderColumn("value", PDFToolTranslationContext::tr("Value"));
        formatter.writeTableHeaderColumn("page", PDFToolTranslationContext::tr("Page"));
        formatter.writeTableHeaderColumn("rect", PDFToolTranslationContext::tr("Rect"));
        formatter.writeTableHeaderColumn("readonly", PDFToolTranslationContext::tr("Read only"));
        formatter.endTableHeaderRow();

        for (const pdf::PDFFormFieldPointer& field : form.getFormFields())
        {
            listFieldRecursive(field.get(),
                               field->getName(pdf::PDFFormField::FullyQualified),
                               formatter,
                               &document,
                               document.getCatalog(),
                               0);
        }

        formatter.endTable();
    }

    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);
    return ExitSuccess;
}

} // namespace pdftool
