// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// Helpers for tests that build isolated temporary Git repositories. Git runs with system and
// global configuration disabled so that tests never read a contributor's personal settings.
namespace GitTestSupport
{

inline void isolateGitConfiguration()
{
    qputenv("GIT_CONFIG_NOSYSTEM", QByteArrayLiteral("1"));
    qputenv("GIT_CONFIG_GLOBAL", QByteArrayLiteral("/dev/null"));
    qputenv("GIT_TERMINAL_PROMPT", QByteArrayLiteral("0"));
}

[[nodiscard]] inline bool runGit(const QString& workingDirectory, const QStringList& arguments)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("/dev/null"));
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(workingDirectory);
    process.setProgram(QStringLiteral(GIT_EXECUTABLE_PATH));
    QStringList completeArguments{
        QStringLiteral("-c"), QStringLiteral("user.name=LinuxGitShell Tests"),
        QStringLiteral("-c"), QStringLiteral("user.email=tests@linuxgitshell.invalid"),
        QStringLiteral("-c"), QStringLiteral("protocol.file.allow=always")};
    completeArguments.append(arguments);
    process.setArguments(completeArguments);
    process.start();
    return process.waitForStarted(5000) && process.waitForFinished(10000) &&
           process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

[[nodiscard]] inline bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(contents) == contents.size();
}

// Creates a repository with one commit at `path`.
[[nodiscard]] inline bool createRepository(const QString& path)
{
    return QDir().mkpath(path) && runGit(path, {QStringLiteral("init"), QStringLiteral(".")}) &&
           writeTextFile(QDir(path).filePath(QStringLiteral("file.txt")), "content\n") &&
           runGit(path, {QStringLiteral("add"), QStringLiteral("file.txt")}) &&
           runGit(path,
                  {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Initial")});
}

} // namespace GitTestSupport
