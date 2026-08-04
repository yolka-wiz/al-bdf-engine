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

#include "pdftoolsign.h"

#include "pdfcatalog.h"
#include "pdfcertificatemanager.h"
#include "pdfcertificatestore.h"
#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfoutputformatter.h"

#include <QBuffer>
#include <QFile>

namespace pdftool
{

static PDFToolSign s_signApplication;

QString PDFToolSign::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
    case Command:
        return "sign";

    case Name:
        return PDFToolTranslationContext::tr("Sign");

    case Description:
        return PDFToolTranslationContext::tr("Apply a digital signature (PKCS#7 detached).");

    default:
        Q_ASSERT(false);
        break;
    }

    return QString();
}

PDFToolAbstractApplication::Options PDFToolSign::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | Sign;
}

int PDFToolSign::execute(const PDFToolOptions& options)
{
    if (options.signCertificateFile.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Certificate file is required (--cert)."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }
    if (options.signOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document filename is required."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    // Load the PKCS#12 certificate.
    QFile certificateFile(options.signCertificateFile);
    if (!certificateFile.open(QIODevice::ReadOnly))
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Cannot open certificate file '%1'.").arg(options.signCertificateFile),
            options.outputCodec);
        return ErrorInvalidArguments;
    }
    pdf::PDFCertificateEntry certificateEntry;
    certificateEntry.pkcs12 = certificateFile.readAll();
    certificateEntry.pkcs12fileName = options.signCertificateFile;
    certificateFile.close();

    pdf::PDFDocument document;
    if (!readDocument(options, document, nullptr, false))
    {
        return ErrorDocumentReading;
    }

    // First signature pass: verify the private key works by signing a probe.
    // (The real signature is computed after the byte ranges are known.)
    QByteArray probe;
    if (!pdf::PDFSignatureFactory::sign(certificateEntry, options.signPassword, QByteArray("probe"), probe))
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Failed to load private key from certificate (wrong password?)."),
            options.outputCodec);
        return ErrorInvalidArguments;
    }

    const pdf::PDFInteger offsetMark = 123456789123;
    const QByteArray offsetMarkString = QByteArray::number(offsetMark);
    const int offsetMarkStringLength = offsetMarkString.length();

    // Build the signature field. The placeholder contents is the probe
    // signature from the same certificate — a PKCS#7 detached signature size
    // depends only on the key and certificate chain, not on the signed data,
    // so the final signature fits exactly into the reserved space and the
    // byte range stays valid.
    pdf::PDFDocumentBuilder builder(&document);
    pdf::PDFObjectReference signatureDictionary = builder.createSignatureDictionary(
        "Adobe.PPKLite", "adbe.pkcs7.detached", probe, QDateTime::currentDateTime(), offsetMark);

    const QString signatureName =
        QStringLiteral("pdfedit_signature_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    pdf::PDFObjectReference formField = builder.createFormFieldSignature(signatureName, {}, signatureDictionary);
    builder.createAcroForm({formField});

    const pdf::PDFCatalog* catalog = document.getCatalog();
    const bool visible = !options.signPage.isEmpty() && !options.signX.isEmpty() && !options.signY.isEmpty() &&
                         !options.signW.isEmpty() && !options.signH.isEmpty();

    if (visible)
    {
        bool ok = false;
        const pdf::PDFInteger pageNumber = options.signPage.toInt(&ok);
        if (!ok || pageNumber < 1 || pageNumber > pdf::PDFInteger(catalog->getPageCount()))
        {
            PDFConsole::writeError(PDFToolTranslationContext::tr("Invalid page number '%1'.").arg(options.signPage),
                                   options.outputCodec);
            return ErrorInvalidArguments;
        }
        const pdf::PDFObjectReference pageReference = catalog->getPage(pageNumber - 1)->getPageReference();
        const QRectF rect(
            options.signX.toDouble(), options.signY.toDouble(), options.signW.toDouble(), options.signH.toDouble());
        builder.createFormFieldWidget(formField, pageReference, {}, rect);
    }
    else
    {
        if (catalog->getPageCount() > 0)
        {
            builder.createInvisibleFormFieldWidget(formField, catalog->getPage(0)->getPageReference());
        }
    }

    if (!options.signReason.isEmpty())
    {
        builder.setSignatureReason(signatureDictionary, options.signReason);
    }
    if (!options.signContactInfo.isEmpty())
    {
        builder.setSignatureContactInfo(signatureDictionary, options.signContactInfo);
    }

    pdf::PDFDocument signedDocument = builder.build();

    // 1) Write the document with the placeholder signature.
    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);
    pdf::PDFDocumentWriter writer(nullptr);
    writer.write(&buffer, &signedDocument);

    const QByteArray bufferData = buffer.data();
    const QByteArray probeHex = probe.toHex();
    const int indexOfSignature = bufferData.indexOf(probeHex);
    if (indexOfSignature == -1)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to locate signature placeholder in output."),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    // 2) Compute the byte ranges around the signature contents.
    //    ByteRange = [0, i2] + [i3, i4]; the gap [i2+1, i3-1] is the
    //    signature hex contents (reserved).
    const pdf::PDFInteger i2 = indexOfSignature - 1;
    const pdf::PDFInteger i3 = i2 + probeHex.size() + 2;
    const pdf::PDFInteger i4 = bufferData.size() - i3;

    auto writeInt = [&](pdf::PDFInteger offset) {
        const QString offsetString = QString::number(offset).leftJustified(offsetMarkStringLength, ' ', true);
        // NOTE: use the LIVE buffer data — each patch consumes one placeholder
        // (lastIndexOf walks backward through the four ByteRange entries).
        const int index = buffer.data().lastIndexOf(offsetMarkString, indexOfSignature);
        if (index < 0)
        {
            return;
        }
        buffer.seek(index);
        buffer.write(offsetString.toLatin1());
    };

    // 3) Patch the four ByteRange integers in place (fixed-width).
    writeInt(i4);
    writeInt(i3);
    writeInt(i2);
    writeInt(0);

    // 4) Sign the covered byte ranges.
    QByteArray dataToBeSigned;
    buffer.seek(0);
    dataToBeSigned.append(buffer.read(i2));
    buffer.seek(i3);
    dataToBeSigned.append(buffer.read(i4));

    QByteArray signature;
    if (!pdf::PDFSignatureFactory::sign(certificateEntry, options.signPassword, dataToBeSigned, signature))
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to create digital signature."),
                               options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    // 5) Write the signature hex into the reserved contents gap.
    buffer.seek(i2 + 1);
    buffer.write(signature.toHex());
    buffer.close();

    QFile outputFile(options.signOutputDocument);
    if (!outputFile.open(QIODevice::WriteOnly))
    {
        PDFConsole::writeError(
            PDFToolTranslationContext::tr("Cannot open output file '%1' for writing.").arg(options.signOutputDocument),
            options.outputCodec);
        return ErrorFailedWriteToFile;
    }
    outputFile.write(buffer.data());
    outputFile.close();

    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("sign", PDFToolTranslationContext::tr("Sign"));
    formatter.writeText("signed",
                        PDFToolTranslationContext::tr("Document signed (%1-byte signature).").arg(signature.size()));
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);

    return ExitSuccess;
}

} // namespace pdftool
