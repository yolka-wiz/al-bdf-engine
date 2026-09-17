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

#include "pdftoolabstractapplication.h"
#include "pdfconstants.h"
#include "pdfexception.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);
    QCoreApplication::setOrganizationName("MelkaJ");
    QCoreApplication::setApplicationName("albdf");
    QCoreApplication::setApplicationVersion(pdf::PDF_LIBRARY_VERSION);

    const QString versionText =
        QCoreApplication::applicationName() + QLatin1Char(' ') + QCoreApplication::applicationVersion();

    QStringList arguments = QCoreApplication::arguments();

    QCommandLineParser parser;
    parser.setApplicationDescription("albdf - work with pdf documents via command line");
    parser.addPositionalArgument("command", "Command to execute.");
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    const auto writeStdout = [](const QString& text) {
        QTextStream stream(stdout);
        stream << text << Qt::endl;
    };
    const auto writeStderr = [](const QString& text) {
        QTextStream stream(stderr);
        stream << text << Qt::endl;
    };

    // The command is the first non-option token. Locate it in the raw argument
    // list so exactly that token is dropped below: removing by value (the old
    // QStringList::removeOne) could delete a later positional that happens to
    // be equal to the command.
    int commandIndex = -1;
    for (int i = 1; i < arguments.size(); ++i)
    {
        if (!arguments.at(i).startsWith(QLatin1Char('-')))
        {
            commandIndex = i;
            break;
        }
    }
    const QString command = commandIndex > 0 ? arguments.at(commandIndex) : QString();

    try
    {
        // First pass: only the global --help/--version options and the command
        // positional are known, so unrecognized command options are not an
        // error yet. The command-specific parser is installed afterwards and
        // the second parse diagnoses malformed options.
        const bool globalParseOk = parser.parse(arguments);

        if (command.isEmpty())
        {
            if (parser.isSet(versionOption))
            {
                writeStdout(versionText);
                return pdftool::PDFToolAbstractApplication::ExitSuccess;
            }

            if (globalParseOk || parser.isSet(helpOption) || parser.isSet(QStringLiteral("help-all")))
            {
                writeStdout(parser.helpText());
                return pdftool::PDFToolAbstractApplication::ExitSuccess;
            }

            writeStderr(parser.errorText());
            writeStderr(parser.helpText());
            return pdftool::PDFToolAbstractApplication::ErrorInvalidArguments;
        }

        pdftool::PDFToolAbstractApplication* application =
            pdftool::PDFToolApplicationStorage::getApplicationByCommand(command);
        if (!application)
        {
            writeStderr(QStringLiteral("Unknown command '%1'").arg(command));
            writeStderr(parser.helpText());
            return pdftool::PDFToolAbstractApplication::ErrorInvalidArguments;
        }

        arguments.removeAt(commandIndex);
        parser.clearPositionalArguments();
        application->initializeCommandLineParser(&parser);

        if (!parser.parse(arguments))
        {
            writeStderr(parser.errorText());
            writeStderr(parser.helpText());
            return pdftool::PDFToolAbstractApplication::ErrorInvalidArguments;
        }

        if (parser.isSet(versionOption))
        {
            writeStdout(versionText);
            return pdftool::PDFToolAbstractApplication::ExitSuccess;
        }

        // addHelpOption() registers two options: "-h/--help" and "--help-all"
        // (the latter adds generic Qt options upstream). process() handled both;
        // since we parse manually, treat "--help-all" as help too or a command
        // would silently run when only help was requested.
        if (parser.isSet(helpOption) || parser.isSet(QStringLiteral("help-all")))
        {
            writeStdout(parser.helpText());
            return pdftool::PDFToolAbstractApplication::ExitSuccess;
        }

        return application->execute(application->getOptions(&parser));
    }
    catch (const pdf::PDFException& exception)
    {
        writeStderr(QStringLiteral("Error: %1").arg(exception.getMessage()));
        return pdftool::PDFToolAbstractApplication::ErrorUnknown;
    }
    catch (const std::exception& exception)
    {
        writeStderr(QStringLiteral("Error: %1").arg(QString::fromLocal8Bit(exception.what())));
        return pdftool::PDFToolAbstractApplication::ErrorUnknown;
    }
    catch (...)
    {
        writeStderr(QStringLiteral("Error: unknown exception"));
        return pdftool::PDFToolAbstractApplication::ErrorUnknown;
    }
}
