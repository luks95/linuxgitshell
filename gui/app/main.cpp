// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include "linuxgitshell/gitcore/Logging.h"
#include "linuxgitshell/gitcore/ProjectInfo.h"

#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    const QByteArray translationDomain = QByteArrayLiteral("linuxgitshell");
    const QDir executableDirectory(QCoreApplication::applicationDirPath());
    KLocalizedString::addDomainLocaleDir(
        translationDomain, executableDirectory.absoluteFilePath(QStringLiteral("../share/locale")));
    KLocalizedString::setApplicationDomain(translationDomain);

    QCoreApplication::setApplicationName(QStringLiteral("linuxgitshell"));
    QCoreApplication::setApplicationVersion(LinuxGitShell::ProjectInfo::version());

    const KAboutData aboutData(QStringLiteral("linuxgitshell"), LinuxGitShell::ProjectInfo::name(),
                               LinuxGitShell::ProjectInfo::version());
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Native Git desktop integration for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("repository"), i18n("Repository path to inspect."),
                                 QStringLiteral("[repository]"));
    parser.process(application);

    const QStringList positionalArguments = parser.positionalArguments();
    const QString requestedPath = positionalArguments.value(0);

    qCInfo(LinuxGitShell::gitCoreLog) << "Starting LinuxGitShell";

    LinuxGitShell::MainWindow window(requestedPath);
    window.show();

    return application.exec();
}
