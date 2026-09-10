// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <cstdio>
#ifdef Q_OS_UNIX
#include <csignal>
#endif

namespace
{

void writeBytes(FILE* stream, const QByteArray& bytes)
{
    std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stream);
    std::fflush(stream);
}

int context(const QStringList& arguments)
{
    if (arguments.size() < 3)
    {
        return 2;
    }

    QJsonArray forwardedArguments;
    for (qsizetype index = 3; index < arguments.size(); ++index)
    {
        forwardedArguments.append(arguments.at(index));
    }

    QJsonObject object;
    const QByteArray environmentName = arguments.at(2).toUtf8();
    object.insert(QStringLiteral("arguments"), forwardedArguments);
    object.insert(QStringLiteral("environment"), qEnvironmentVariable(environmentName.constData()));
    object.insert(QStringLiteral("workingDirectory"), QDir::currentPath());
    writeBytes(stdout, QJsonDocument(object).toJson(QJsonDocument::Compact));
    return 0;
}

int waitForTermination()
{
#ifdef Q_OS_UNIX
    std::signal(SIGTERM, SIG_IGN);
#endif
    writeBytes(stdout, QByteArrayLiteral("ready"));
    QThread::sleep(30);
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() < 2)
    {
        return 2;
    }

    const QString mode = arguments.at(1);
    if (mode == QStringLiteral("--context"))
    {
        return context(arguments);
    }
    if (mode == QStringLiteral("--streams"))
    {
        writeBytes(stdout, QByteArrayLiteral("stdout-data"));
        writeBytes(stderr, QByteArrayLiteral("stderr-data"));
        return 0;
    }
    if (mode == QStringLiteral("--exit") && arguments.size() == 3)
    {
        bool valid = false;
        const int exitCode = arguments.at(2).toInt(&valid);
        return valid ? exitCode : 2;
    }
    if (mode == QStringLiteral("--wait"))
    {
        return waitForTermination();
    }

    return 2;
}
