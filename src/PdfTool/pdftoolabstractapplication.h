// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PDFTOOLABSTRACTAPPLICATION_H
#define PDFTOOLABSTRACTAPPLICATION_H

#include "pdfcms.h"
#include "pdfdocument.h"
#include "pdfdocumenttextflow.h"
#include "pdfglobal.h"
#include "pdfimageoptimizer.h"
#include "pdfoptimizer.h"
#include "pdfoutputformatter.h"
#include "pdfredact.h"
#include "pdfrenderer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringConverter>
#include <QtGlobal>

#include <vector>

class QCommandLineParser;

namespace pdftool
{

struct PDFToolTranslationContext
{
    Q_DECLARE_TR_FUNCTIONS(PDFToolTranslationContext)
};

struct PDFToolOptions
{
    enum DateFormat
    {
        LocaleShortDate,
        LocaleLongDate,
        ISODate,
        RFC2822Date
    };

    // For option 'ConsoleFormat'
    PDFOutputFormatter::Style outputStyle = PDFOutputFormatter::Style::Text;
    QStringConverter::Encoding outputCodec = QStringConverter::Utf8;

    // For option 'DateFormat'
    DateFormat outputDateFormat = LocaleShortDate;

    // For option 'OpenDocument'
    QString document;
    QString password;
    bool permissiveReading = true;

    // For option 'SignatureVerification'
    bool verificationUseUserCertificates = true;
    bool verificationUseSystemCertificates = true;
    bool verificationOmitCertificateCheck = false;
    bool verificationPrintCertificateDetails = false;
    bool verificationIgnoreExpirationDate = false;

    // For option 'XMLExport'
    bool xmlExportStreams = false;
    bool xmlExportStreamsAsText = false;
    bool xmlUseIndent = false;
    bool xmlAlwaysBinaryStrings = false;

    // For option 'Attachments'
    QString attachmentsSaveNumber;
    QString attachmentsSaveFileName;
    QString attachmentsOutputDirectory;
    QString attachmentsTargetFile;
    bool attachmentsSaveAll = false;

    // For option 'ComputeHashes'
    bool computeHashes = false;

    // For option 'PageSelector'
    QString pageSelectorFirstPage;
    QString pageSelectorLastPage;
    QString pageSelectorSelection;

    // For option 'TextAnalysis'
    pdf::PDFDocumentTextFlowFactory::Algorithm textAnalysisAlgorithm = pdf::PDFDocumentTextFlowFactory::Algorithm::Auto;

    // For option 'TextShow'
    bool textShowPageNumbers = false;
    bool textShowStructTitles = false;
    bool textShowStructLanguage = false;
    bool textShowStructAlternativeDescription = false;
    bool textShowStructExpandedForm = false;
    bool textShowStructActualText = false;
    bool textShowStructPhoneme = false;

    // For option 'VoiceSelector'
    QString textVoiceName;
    QString textVoiceGender;
    QString textVoiceAge;
    QString textVoiceLangCode;

    // For option 'TextSpeech'
    bool textSpeechMarkPageNumbers = false;
    bool textSpeechSayPageNumbers = false;
    bool textSpeechSayStructTitles = false;
    bool textSpeechSayStructAlternativeDescription = false;
    bool textSpeechSayStructExpandedForm = false;
    bool textSpeechSayStructActualText = false;
    QString textSpeechAudioFormat = "mp3";

    // For option 'CharacterMaps'
    bool showCharacterMapsForEmbeddedFonts = false;

    // For option 'ImageWriterSettings'
    pdf::PDFImageWriterSettings imageWriterSettings;

    // For option 'ImageExportSettings'
    pdf::PDFPageImageExportSettings imageExportSettings;

    // For option 'ColorManagementSystem'
    pdf::PDFCMSSettings cmsSettings;

    // For option 'RenderFlags'
    pdf::PDFRenderer::Features renderFeatures = pdf::PDFRenderer::getDefaultFeatures();
    bool renderUseSoftwareRendering = true;
    bool renderShowPageStatistics = false;
    int renderMSAAsamples = 4;
    int renderRasterizerCount = pdf::PDFRasterizerPool::getDefaultRasterizerCount();

    // For option 'Separate'
    QString separatePagePattern;

    // For option 'Unite'
    QStringList uniteFiles;

    // For option 'Diff'
    QStringList diffFiles;

    // For option 'Optimize'
    pdf::PDFOptimizer::OptimizationFlags optimizeFlags = pdf::PDFOptimizer::None;
    pdf::PDFImageOptimizer::Settings imageOptimizationSettings = pdf::PDFImageOptimizer::Settings::createDefault();

    // For option 'CertStore'
    bool certStoreEnumerateSystemCertificates = false;
    bool certStoreEnumerateUserCertificates = true;

    // For option 'CertStoreInstall'
    QString certificateStoreInstallCertificateFile;

    // For option 'Redact'
    pdf::PDFRedact::Options redactOptions = {};
    QString redactedDocument;

    // For option 'DeleteObject'
    QString deleteObjectPage;           ///< Page number (1-based) to edit
    QString deleteObjectIndex;          ///< Object index on the page (0-based, recognize-text contract)
    bool deleteObjectList = false;      ///< List objects instead of deleting
    QString deleteObjectOutputDocument; ///< Output document filename

    // For option 'AddText'
    QString addTextPage;           ///< Page number (1-based)
    QString addTextX;              ///< X position in PDF points
    QString addTextY;              ///< Y position in PDF points
    QString addText;               ///< Text to add
    QString addTextFontSize;       ///< Font size
    QString addTextOutputDocument; ///< Output document filename
    bool addTextRTL = false;       ///< Use the RTL pipeline (HarfBuzz/FriBidi)
    QString addTextFont;           ///< TTF font file for RTL text
    QString addTextLanguage;       ///< Language tag: fa|ar|he|ur

    // For option 'SearchText'
    QString searchTextQuery;              ///< Query string (logical order)
    bool searchTextCaseSensitive = false; ///< Case-sensitive matching
    bool searchTextNoNormalize = false;   ///< Disable RTL normalization

    // For option 'FormFill'
    QStringList formFillFields;     ///< Field fully qualified names (paired with values)
    QStringList formFillValues;     ///< Values for the fields
    QString formFillOutputDocument; ///< Output document filename
    QString formFillFont;            ///< TTF font file for RTL appearance streams

    // For option 'Sign'
    QString signCertificateFile;        ///< PKCS#12 certificate file (.p12/.pfx)
    QString signPassword;               ///< Private key password
    QString signReason;                 ///< Reason for signing (optional)
    QString signContactInfo;            ///< Contact info (optional)
    QString signPage;                   ///< Page for a visible signature (optional; default: invisible)
    QString signX, signY, signW, signH; ///< Visible signature widget rect
    QString signOutputDocument;         ///< Output document filename
    // For option 'Encrypt'
    pdf::PDFSecurityHandlerFactory::Algorithm encryptionAlgorithm = pdf::PDFSecurityHandlerFactory::Algorithm::AES_256;
    pdf::PDFSecurityHandlerFactory::EncryptContents encryptionContents =
        pdf::PDFSecurityHandlerFactory::EncryptContents::All;
    QString encryptionUserPassword;
    QString encryptionOwnerPassword;
    uint32_t encryptionPermissions = 0;

    // For option 'Rotate'
    QStringList rotatePages;      ///< Page numbers (1-based) to rotate
    QString rotateAngle;          ///< Rotation angle in degrees: 90, 180 or 270 (clockwise)
    QString rotateOutputDocument; ///< Output document filename

    // For option 'MovePage'
    QString movePageFrom;           ///< Source page number (1-based)
    QString movePageTo;             ///< Target position (1-based)
    QString movePageOutputDocument; ///< Output document filename

    // For option 'DeletePage'
    QStringList deletePagePages;      ///< Page numbers (1-based) to delete
    QString deletePageSelection;      ///< Page selection, e.g. '1,3' or '2-4'
    QString deletePageOutputDocument; ///< Output document filename

    /// Returns page range. If page range is invalid, then \p errorMessage is empty.
    /// \param pageCount Page count
    /// \param[out] errorMessage Error message
    /// \param zeroBased Convert to zero based page range?
    std::vector<pdf::PDFInteger> getPageRange(pdf::PDFInteger pageCount, QString& errorMessage, bool zeroBased) const;

    struct RenderFeatureInfo
    {
        QString option;
        QString description;
        pdf::PDFRenderer::Feature feature;
    };

    /// Returns a list of available renderer features
    static std::vector<RenderFeatureInfo> getRenderFeatures();

    struct OptimizeFeatureInfo
    {
        QString option;
        QString description;
        pdf::PDFOptimizer::OptimizationFlag flag;
    };

    /// Returns a list of available optimize features
    static std::vector<OptimizeFeatureInfo> getOptimizeFlagInfos();
};

/// Base class for all applications
class PDFToolAbstractApplication
{
public:
    explicit PDFToolAbstractApplication(bool isDefault = false);
    virtual ~PDFToolAbstractApplication() = default;

    enum ExitCodes
    {
        ExitSuccess = EXIT_SUCCESS,
        ExitFailure = EXIT_FAILURE,
        ErrorUnknown,
        ErrorNoDocumentSpecified,
        ErrorDocumentReading,
        ErrorDocumentWriting,
        ErrorCertificateReading,
        ErrorInvalidArguments,
        ErrorFailedWriteToFile,
        ErrorPermissions,
        ErrorNoText,
        ErrorCOM,
        ErrorSAPI,
        ErrorEncryptionSettings
    };

    enum StandardString
    {
        Command,    ///< Command, by which is this application invoked
        Name,       ///< Name of application
        Description ///< Description (what this application does)
    };

    enum Option
    {
        ConsoleFormat = 0x00000001,                 ///< Set format of console output (text, xml or html)
        OpenDocument = 0x00000002,                  ///< Flags for document reading
        SignatureVerification = 0x00000004,         ///< Flags for signature verification,
        XmlExport = 0x00000008,                     ///< Flags for xml export
        Attachments = 0x00000010,                   ///< Flags for attachments manipulating
        DateFormat = 0x00000020,                    ///< Date format
        ComputeHashes = 0x00000040,                 ///< Compute hashes
        PageSelector = 0x00000080,                  ///< Select page range (or all pages)
        TextAnalysis = 0x00000100,                  ///< Text analysis options
        TextShow = 0x00000200,                      ///< Text extract and show options
        VoiceSelector = 0x00000400,                 ///< Select voice from SAPI
        TextSpeech = 0x00000800,                    ///< Text speech options
        CharacterMaps = 0x00001000,                 ///< Character maps for embedded fonts
        ImageWriterSettings = 0x00002000,           ///< Settings for writing images (for example, format, etc.)
        ImageExportSettingsFiles = 0x00004000,      ///< Settings for exporting page images to files
        ImageExportSettingsResolution = 0x00008000, ///< Settings for resolution of exported images
        ColorManagementSystem = 0x00010000,         ///< Color management system settings
        RenderFlags = 0x00020000,                   ///< Render flags for page image rasterizer
        Separate = 0x00040000,                      ///< Settings for Separate tool
        Unite = 0x00080000,                         ///< Settings for Unite tool
        Optimize = 0x00100000,                      ///< Settings for Optimize tool
        CertStore = 0x00200000,                     ///< Settings for certificate store tool
        CertStoreInstall = 0x00400000,              ///< Settings for certificate store install certificate tool
        Encrypt = 0x00800000,                       ///< Encryption settings
        Diff = 0x01000000,                          ///< Diff settings (compare documents)
        Redact = 0x02000000,                        ///< Settings for Redact tool
        DeleteObject = 0x04000000,                  ///< Settings for DeleteObject tool
        AddText = 0x08000000,                       ///< Settings for AddText tool
        SearchText = 0x10000000,                    ///< Settings for SearchText tool
        FormFill = 0x20000000,                      ///< Settings for FormFill tool
        Sign = 0x40000000,                          ///< Settings for Sign tool
        Rotate = 0x80000000,                        ///< Settings for Rotate tool
        MovePage = 0x100000000,                     ///< Settings for MovePage tool
        DeletePage = 0x200000000,                   ///< Settings for DeletePage tool
    };

    // 64-bit flag storage. Qt's QFlags is int-backed (static_assert in Qt 6.10
    // rejects enums wider than int), and the Option enum now has 34 flags —
    // bits 32 and 33 (MovePage, DeletePage) overflow QFlags. This class keeps
    // the QFlags API surface used by the CLI (testFlag, operator|) while
    // allowing flags above bit 31.
    class Options
    {
    public:
        constexpr Options() noexcept = default;
        constexpr Options(Option flag) noexcept : m_flags(static_cast<quint64>(flag)) {}

        constexpr bool testFlag(Option flag) const noexcept
        {
            return (m_flags & static_cast<quint64>(flag)) != 0;
        }

        constexpr bool operator==(const Options& other) const noexcept
        {
            return m_flags == other.m_flags;
        }

        constexpr bool operator!=(const Options& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr Options operator|(Option flag) const noexcept
        {
            return Options(m_flags | static_cast<quint64>(flag));
        }

        constexpr Options operator|(const Options& other) const noexcept
        {
            return Options(m_flags | other.m_flags);
        }

        constexpr Options& operator|=(Option flag) noexcept
        {
            m_flags |= static_cast<quint64>(flag);
            return *this;
        }

        constexpr Options& operator|=(const Options& other) noexcept
        {
            m_flags |= other.m_flags;
            return *this;
        }

    private:
        constexpr explicit Options(quint64 flags) noexcept : m_flags(flags) {}

        quint64 m_flags = 0;
    };

    virtual QString getStandardString(StandardString standardString) const = 0;
    virtual int execute(const PDFToolOptions& options) = 0;
    virtual Options getOptionsFlags() const = 0;

    void initializeCommandLineParser(QCommandLineParser* parser) const;
    PDFToolOptions getOptions(QCommandLineParser* parser) const;

    static QString convertDateTimeToString(const QDateTime& dateTime, PDFToolOptions::DateFormat dateFormat);

protected:
    /// Tries to read the document. If document is successfully read, true is returned,
    /// if error occurs, then false is returned. Optionally, original document content
    /// can also be retrieved.
    /// \param options Options
    /// \param document Document
    /// \param[out] sourceData Pointer, to which source data are stored
    /// \param authorizeOwnerOnly Require to authorize as owner
    bool readDocument(const PDFToolOptions& options,
                      pdf::PDFDocument& document,
                      QByteArray* sourceData,
                      bool authorizeOwnerOnly);

    /// Returns a list of available encodings
    static QList<QByteArray> getAvailableEncodings();

    /// Returns default encoding
    static QByteArray getDefaultEncoding();

    /// Converts string to encoding
    static QStringConverter::Encoding getEncoding(const QString& encodingName);
};

/// This class stores information about all applications available. Application
/// can be selected by command string. Also, default application is available.
class PDFToolApplicationStorage
{
public:
    /// Returns application by command. If application for given command is not found,
    /// then nullptr is returned.
    /// \param command Command
    /// \returns Application for given command or nullptr
    static PDFToolAbstractApplication* getApplicationByCommand(const QString& command);

    /// Registers application to the application storage. If \p isDefault
    /// is set to true, application is registered as default application.
    /// \param application Apllication
    /// \param isDefault Is default application?
    static void registerApplication(PDFToolAbstractApplication* application, bool isDefault = false);

    /// Returns default application
    /// \returns Default application
    static PDFToolAbstractApplication* getDefaultApplication();

    /// Returns a list of available applications
    static const std::vector<PDFToolAbstractApplication*>& getApplications();

private:
    PDFToolApplicationStorage() = default;
    static PDFToolApplicationStorage* getInstance();

    std::vector<PDFToolAbstractApplication*> m_applications;
    PDFToolAbstractApplication* m_defaultApplication = nullptr;
};

} // namespace pdftool

// Free operators so `Option | Option` (e.g. `ConsoleFormat | OpenDocument`)
// composes like the old QFlags API.
//
// IMPORTANT: these must live at GLOBAL scope, NOT inside namespace pdftool.
// Name lookup inside namespace pdftool walks outward until it finds the first
// scope declaring `operator|`; a pdftool-scope operator| would SHADOW the
// QFlags operator| declarations that Qt places at global scope (and that
// other code in this namespace relies on — e.g. `PDFOptimizer::Flags | Flags`
// in pdftooldeletepage.cpp). At global scope both coexist and overload
// resolution picks the exact match.
inline constexpr pdftool::PDFToolAbstractApplication::Options operator|(pdftool::PDFToolAbstractApplication::Option left,
                                                                        pdftool::PDFToolAbstractApplication::Option right)
{
    return pdftool::PDFToolAbstractApplication::Options(left) | right;
}

inline constexpr pdftool::PDFToolAbstractApplication::Options operator|(pdftool::PDFToolAbstractApplication::Option left,
                                                                        const pdftool::PDFToolAbstractApplication::Options& right)
{
    return pdftool::PDFToolAbstractApplication::Options(left) | right;
}

#endif // PDFTOOLABSTRACTAPPLICATION_H
