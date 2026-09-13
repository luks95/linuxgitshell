// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QFile>
#include <QStringList>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString outputPath = qEnvironmentVariable("LINUXGITSHELL_PLUGIN_TEST_OUTPUT");
    const QStringList arguments = application.arguments();
    if (outputPath.isEmpty() || arguments.size() != 2)
    {
        return 1;
    }

    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return 2;
    }

    const QByteArray argument = arguments.at(1).toUtf8();
    if (output.write(argument) != argument.size())
    {
        return 3;
    }

    return 0;
}
