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

#include "pdftoolformfill.h"

#include "pdfcatalog.h"
#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfform.h"
#include "pdfoutputformatter.h"

#include <QFile>

namespace pdftool
{

static PDFToolFormFill s_formFillApplication;

QString PDFToolFormFill::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "form-fill";

    case Name:
        return PDFToolTranslationContext::tr("Form fill");

    case Description:
        return PDFToolTranslationContext::tr("Set values of interactive form fields.");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

PDFToolAbstractApplication::Options PDFToolFormFill::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | FormFill;
}

namespace
{

/// Recursively find a field by fully qualified name.
pdf::PDFFormField* findFieldByName(pdf::PDFFormField* field, const QString& name)
{
    if (field->getName(pdf::PDFFormField::FullyQualified) == name)
    {
        return field;
    }
    const pdf::PDFFormFields& children = field->getChildFields();
    for (const pdf::PDFFormFieldPointer& child : children)
    {
        if (pdf::PDFFormField* found = findFieldByName(child.get(), name))
        {
            return found;
        }
    }
    return nullptr;
}

/// Build the value object appropriate for the field type.
pdf::PDFObject makeValueObject(const pdf::PDFFormField* field, const QString& value)
{
    switch (field->getFieldType())
    {
    case pdf::PDFFormField::FieldType::Button:
        // Button values are names: "On"/"Off" (checkbox/radio states).
        return pdf::PDFObject::createName(value.toLatin1());

    case pdf::PDFFormField::FieldType::Text:
    case pdf::PDFFormField::FieldType::Choice:
    case pdf::PDFFormField::FieldType::Signature:
    default:
        return pdf::PDFObject::createString(value.toUtf8());
    }
}

} // namespace

int PDFToolFormFill::execute(const PDFToolOptions& options)
{
    if (options.formFillFields.size() != options.formFillValues.size())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Number of --field and --value options must match."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    if (options.formFillFields.isEmpty())
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("No fields specified. Use --field <name> --value <value>."),
            options.outputCodec);
        return ErrorInvalidArguments;
    }
    if (options.formFillOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename is required."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFDocument document;
    if (!readDocument(options, document, nullptr, false))
    {
        return ErrorDocumentReading;
    }

    // Parse the form against the ORIGINAL document to locate field references.
    const pdf::PDFObject formObject = document.getCatalog()->getFormObject();
    pdf::PDFForm form = pdf::PDFForm::parse(&document, formObject);

    if (form.getFormFields().empty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Document has no form fields."), options.outputCodec);
        return ErrorInvalidArguments;
    }

    // Set up the modification pipeline: the modifier copies the document and
    // applies field-value changes through its builder. The form manager is
    // connected to the ORIGINAL document — setValue only reads widget
    // appearance dicts through it (getOnAppearanceState), and object numbers
    // are stable in the builder's copy, so lookups stay valid.
    pdf::PDFDocumentModifier modifier(&document);
    pdf::PDFFormManager formManager(nullptr);
    formManager.setDocument(pdf::PDFModifiedDocument(&document, nullptr));

    // Optional TTF font for RTL appearance streams. When set, text fields
    // whose value is right-to-left get a shaped appearance stream with an
    // embedded Type0 TrueType subset (see
    // PDFDocumentBuilder::updateAnnotationAppearanceStreams). LTR fills are
    // unaffected.
    if (!options.formFillFont.isEmpty())
    {
        QFile fontFile(options.formFillFont);
        if (!fontFile.open(QIODevice::ReadOnly))
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Cannot open font file '%1'.").arg(options.formFillFont),
                options.outputCodec);
            return ErrorInvalidArguments;
        }
        modifier.getBuilder()->setRtlFormFieldFontData(fontFile.readAll());
    }

    int setCount = 0;
    for (int i = 0; i < options.formFillFields.size(); ++i)
    {
        const QString& fieldName = options.formFillFields.at(i);
        const QString& fieldValue = options.formFillValues.at(i);

        pdf::PDFFormField* field = nullptr;
        const pdf::PDFFormFields& roots = form.getFormFields();
        for (const pdf::PDFFormFieldPointer& root : roots)
        {
            field = findFieldByName(root.get(), fieldName);
            if (field)
            {
                break;
            }
        }

        if (!field)
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Form field '%1' not found.").arg(fieldName),
                                   options.outputCodec);
            return ErrorInvalidArguments;
        }

        pdf::PDFFormField::SetValueParameters parameters;
        parameters.value = makeValueObject(field, fieldValue);
        parameters.modifier = &modifier;
        parameters.formManager = &formManager;
        parameters.scope = pdf::PDFFormField::SetValueParameters::Scope::User;

        if (!field->setValue(parameters))
        {
            PDFConsole::writeError(
                PDFToolTranslationContext::tr("Failed to set value of form field '%1'.").arg(fieldName),
                options.outputCodec);
            return ErrorInvalidArguments;
        }
        ++setCount;
    }

    if (!modifier.finalize())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to finalize document modification."),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    pdf::PDFDocumentWriter writer(nullptr);
    pdf::PDFOperationResult writeResult =
        writer.write(options.formFillOutputDocument, modifier.getDocument().data(), true);
    if (!writeResult)
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Failed to write document: %1").arg(writeResult.getErrorMessage()),
            options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("form-fill", PDFToolTranslationContext::tr("Form fill"));
    formatter.writeText("fields-set", PDFToolTranslationContext::tr("Set %1 form field value(s).").arg(setCount));
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

} // namespace pdftool
