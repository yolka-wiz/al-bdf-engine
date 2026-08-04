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

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

/// Integration tests for the form-list / form-fill / sign commands.
///
/// Runs the PdfTool binary against a synthetic AcroForm PDF generated in a
/// temporary directory, then verifies:
///   - form-list enumerates the fields with their values
///   - form-fill writes new values and form-list reads them back
///   - sign produces a document whose signature verify-signatures accepts
///   - a tampered signed document is rejected (integrity check)
class FormSignatureTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void test_formList();
    void test_formFill();
    void test_signAndVerify();
    void test_signTamperDetected();

private:
    struct ToolResult
    {
        int exitCode = -1;
        QString stdOut;
        QString stdErr;
    };

    ToolResult runTool(const QString& toolPath, const QStringList& arguments, const QString& workDir) const;

    QString m_formPdf;
    QString m_certP12;
    QTemporaryDir m_tmpDir;
};

void FormSignatureTest::initTestCase()
{
    QVERIFY(m_tmpDir.isValid());
    m_formPdf = m_tmpDir.filePath(QStringLiteral("form.pdf"));
    m_certP12 = m_tmpDir.filePath(QStringLiteral("cert.p12"));

    // --- Synthetic AcroForm PDF: one text field "name", one checkbox "agree",
    // one choice "country" ---
    QFile formFile(m_formPdf);
    QVERIFY(formFile.open(QIODevice::WriteOnly));
    const QByteArray formPdf = QByteArrayLiteral(
        "%PDF-1.5\n"
        "1 0 obj\n<< /Type /Catalog /Pages 2 0 R /AcroForm 6 0 R >>\nendobj\n"
        "2 0 obj\n<< /Type /Pages /Kids [4 0 R] /Count 1 >>\nendobj\n"
        "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"
        "4 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 3 0 R >> >> /Contents 5 0 R /Annots [7 0 R 9 0 R] >>\nendobj\n"
        "5 0 obj\n<< /Length 49 >>\nstream\n"
        "BT /F1 12 Tf 1 0 0 1 72 740 Tm (Form test) Tj ET\n"
        "endstream\nendobj\n"
        "6 0 obj\n<< /Fields [8 0 R 10 0 R 12 0 R] /DA (/Helv 0 Tf 0 g) >>\nendobj\n"
        "7 0 obj\n<< /Type /Annot /Subtype /Widget /Rect [100 700 300 720] /P 4 0 R /F 4 >>\nendobj\n"
        "8 0 obj\n<< /FT /Tx /T (name) /V (John) /Type /Annot /Subtype /Widget "
        "/Rect [100 700 300 720] /P 4 0 R /F 4 >>\nendobj\n"
        "9 0 obj\n<< /Type /Annot /Subtype /Widget /Rect [100 600 115 615] /P 4 0 R /F 4 >>\nendobj\n"
        "10 0 obj\n<< /FT /Btn /T (agree) /V /Off /Type /Annot /Subtype /Widget "
        "/Rect [100 600 115 615] /P 4 0 R /F 4 >>\nendobj\n"
        "11 0 obj\n<< /Type /Annot /Subtype /Widget /Rect [100 550 300 565] /P 4 0 R /F 4 >>\nendobj\n"
        "12 0 obj\n<< /FT /Ch /T (country) /V (US) /Opt [(US) (UK) (DE)] /Type /Annot /Subtype /Widget "
        "/Rect [100 550 300 565] /P 4 0 R /F 4 >>\nendobj\n"
        "xref\n"
        "0 13\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000074 00000 n \n"
        "0000000131 00000 n \n"
        "0000000201 00000 n \n"
        "0000000349 00000 n \n"
        "0000000447 00000 n \n"
        "0000000519 00000 n \n"
        "0000000608 00000 n \n"
        "0000000725 00000 n \n"
        "0000000814 00000 n \n"
        "0000000932 00000 n \n"
        "0000001022 00000 n \n"
        "trailer << /Size 13 /Root 1 0 R >>\n"
        "startxref\n"
        "1163\n"
        "%%EOF\n");
    formFile.write(formPdf);
    formFile.close();

    // --- Generate a self-signed PKCS#12 test certificate via OpenSSL ---
    QProcess openssl;
    openssl.start(QStringLiteral("openssl"),
                  {QStringLiteral("req"),
                   QStringLiteral("-x509"),
                   QStringLiteral("-newkey"),
                   QStringLiteral("rsa:2048"),
                   QStringLiteral("-keyout"),
                   m_tmpDir.filePath(QStringLiteral("key.pem")),
                   QStringLiteral("-out"),
                   m_tmpDir.filePath(QStringLiteral("cert.pem")),
                   QStringLiteral("-days"),
                   QStringLiteral("365"),
                   QStringLiteral("-nodes"),
                   QStringLiteral("-subj"),
                   QStringLiteral("/CN=pdfedit test/O=pdfedit")});
    QVERIFY2(openssl.waitForFinished(30000), "openssl req failed");
    QVERIFY2(openssl.exitCode() == 0, qPrintable(openssl.readAllStandardError()));

    openssl.start(QStringLiteral("openssl"),
                  {QStringLiteral("pkcs12"),
                   QStringLiteral("-export"),
                   QStringLiteral("-inkey"),
                   m_tmpDir.filePath(QStringLiteral("key.pem")),
                   QStringLiteral("-in"),
                   m_tmpDir.filePath(QStringLiteral("cert.pem")),
                   QStringLiteral("-out"),
                   m_certP12,
                   QStringLiteral("-passout"),
                   QStringLiteral("pass:testpass")});
    QVERIFY2(openssl.waitForFinished(30000), "openssl pkcs12 failed");
    QVERIFY2(openssl.exitCode() == 0, qPrintable(openssl.readAllStandardError()));
    QVERIFY(QFile::exists(m_certP12));
}

FormSignatureTest::ToolResult
FormSignatureTest::runTool(const QString& toolPath, const QStringList& arguments, const QString& workDir) const
{
    ToolResult result;
    QProcess process;
    process.setWorkingDirectory(workDir);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(toolPath, arguments);
    if (!process.waitForFinished(60000))
    {
        return result;
    }
    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    return result;
}

void FormSignatureTest::test_formList()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    const ToolResult result = runTool(toolPath, {QStringLiteral("form-list"), m_formPdf}, m_tmpDir.path());
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.stdOut.contains(QStringLiteral("name")));
    QVERIFY(result.stdOut.contains(QStringLiteral("John")));
    QVERIFY(result.stdOut.contains(QStringLiteral("agree")));
    QVERIFY(result.stdOut.contains(QStringLiteral("country")));
    QVERIFY(result.stdOut.contains(QStringLiteral("US")));
}

void FormSignatureTest::test_formFill()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    const QString filledPath = m_tmpDir.filePath(QStringLiteral("filled.pdf"));
    const ToolResult fillResult = runTool(toolPath,
                                          {QStringLiteral("form-fill"),
                                           m_formPdf,
                                           filledPath,
                                           QStringLiteral("--field"),
                                           QStringLiteral("name"),
                                           QStringLiteral("--value"),
                                           QStringLiteral("Ali"),
                                           QStringLiteral("--field"),
                                           QStringLiteral("agree"),
                                           QStringLiteral("--value"),
                                           QStringLiteral("On"),
                                           QStringLiteral("--field"),
                                           QStringLiteral("country"),
                                           QStringLiteral("--value"),
                                           QStringLiteral("DE")},
                                          m_tmpDir.path());
    QCOMPARE(fillResult.exitCode, 0);
    QVERIFY(QFile::exists(filledPath));

    const ToolResult listResult = runTool(toolPath, {QStringLiteral("form-list"), filledPath}, m_tmpDir.path());
    QCOMPARE(listResult.exitCode, 0);
    QVERIFY(listResult.stdOut.contains(QStringLiteral("Ali")));
    QVERIFY(listResult.stdOut.contains(QStringLiteral("On")));
    QVERIFY(listResult.stdOut.contains(QStringLiteral("DE")));
    QVERIFY(!listResult.stdOut.contains(QStringLiteral("John"))); // old value gone
}

void FormSignatureTest::test_signAndVerify()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    const QString signedPath = m_tmpDir.filePath(QStringLiteral("signed.pdf"));
    const ToolResult signResult = runTool(toolPath,
                                          {QStringLiteral("sign"),
                                           m_formPdf,
                                           signedPath,
                                           QStringLiteral("--cert"),
                                           m_certP12,
                                           QStringLiteral("--password"),
                                           QStringLiteral("testpass"),
                                           QStringLiteral("--reason"),
                                           QStringLiteral("unit test")},
                                          m_tmpDir.path());
    QCOMPARE(signResult.exitCode, 0);
    QVERIFY(QFile::exists(signedPath));

    const ToolResult verifyResult =
        runTool(toolPath, {QStringLiteral("verify-signatures"), signedPath}, m_tmpDir.path());
    QCOMPARE(verifyResult.exitCode, 0);
    QVERIFY(verifyResult.stdOut.contains(QStringLiteral("Signature")));
    QVERIFY(verifyResult.stdOut.contains(QStringLiteral("OK")));
    QVERIFY(verifyResult.stdOut.contains(QStringLiteral("pdfedit test")));
}

void FormSignatureTest::test_signTamperDetected()
{
    const QString toolPath = QCoreApplication::applicationDirPath() + QStringLiteral("/PdfTool");
    const QString signedPath = m_tmpDir.filePath(QStringLiteral("signed2.pdf"));
    const ToolResult signResult = runTool(toolPath,
                                          {QStringLiteral("sign"),
                                           m_formPdf,
                                           signedPath,
                                           QStringLiteral("--cert"),
                                           m_certP12,
                                           QStringLiteral("--password"),
                                           QStringLiteral("testpass")},
                                          m_tmpDir.path());
    QCOMPARE(signResult.exitCode, 0);
    QVERIFY(QFile::exists(signedPath));
    qDebug() << "signed2 size:" << QFileInfo(signedPath).size() << "stderr:" << signResult.stdErr;

    // Tamper: flip a byte inside the first content stream payload (structure
    // stays valid; the signed bytes change).
    QFile file(signedPath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    const QByteArray data = file.readAll();
    qDebug() << "data size:" << data.size() << "contains stream:" << data.contains("stream")
             << "idx:" << data.indexOf("stream");
    const int streamStart = data.indexOf("stream");
    QVERIFY(streamStart > 0);
    QVERIFY(streamStart + 30 < data.size());
    file.seek(streamStart + 25);
    const char original = data.at(streamStart + 25);
    file.putChar(char(original ^ 0x01));
    file.close();

    const ToolResult verifyResult =
        runTool(toolPath, {QStringLiteral("verify-signatures"), signedPath}, m_tmpDir.path());
    QCOMPARE(verifyResult.exitCode, 0);
    // The tampered row: Certificate=Error, Signature=Error (double Error).
    // An intact signature shows "Error ... OK" (self-signed cert untrusted,
    // but the signature itself validates). Assert the distinguishing pattern.
    QVERIFY(verifyResult.stdOut.contains(QStringLiteral("pdfedit test      Error            Error")));
    QVERIFY(!verifyResult.stdOut.contains(QStringLiteral("pdfedit test      Error            OK")));
}

QTEST_GUILESS_MAIN(FormSignatureTest)
#include "tst_formsignaturetest.moc"
