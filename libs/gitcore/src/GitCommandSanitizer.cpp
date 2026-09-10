// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitCommandSanitizer.h"

#include <QSet>
#include <QUrl>
#include <QUrlQuery>

namespace LinuxGitShell
{
namespace
{

const QString redacted = QStringLiteral("REDACTED");

bool isSensitiveOption(const QString& option)
{
    static const QSet<QString> sensitiveOptions = {
        QStringLiteral("--oauth-token"),
        QStringLiteral("--password"),
        QStringLiteral("--private-token"),
        QStringLiteral("--token"),
    };
    return sensitiveOptions.contains(option.toLower());
}

bool isSensitiveQueryKey(const QString& key)
{
    const QString lowerKey = key.toLower();
    return lowerKey.contains(QStringLiteral("token")) ||
           lowerKey.contains(QStringLiteral("password")) ||
           lowerKey.contains(QStringLiteral("secret"));
}

QString sanitizeUrl(const QString& argument)
{
    QUrl url(argument, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")))
    {
        return argument;
    }

    bool changed = false;
    if (!url.userName().isEmpty() || !url.password().isEmpty())
    {
        url.setUserName(redacted);
        url.setPassword(redacted);
        changed = true;
    }

    QUrlQuery query(url);
    QUrlQuery sanitizedQuery;
    const auto queryItems = query.queryItems(QUrl::FullyDecoded);
    for (const auto& [key, value] : queryItems)
    {
        if (isSensitiveQueryKey(key))
        {
            sanitizedQuery.addQueryItem(key, redacted);
            changed = true;
        }
        else
        {
            sanitizedQuery.addQueryItem(key, value);
        }
    }
    if (changed)
    {
        url.setQuery(sanitizedQuery);
        return url.toString(QUrl::FullyEncoded);
    }

    return argument;
}

} // namespace

QStringList sanitizeGitArguments(const QStringList& arguments)
{
    QStringList sanitized;
    sanitized.reserve(arguments.size());

    bool redactNextValue = false;
    for (const QString& argument : arguments)
    {
        if (redactNextValue)
        {
            sanitized.append(redacted);
            redactNextValue = false;
            continue;
        }

        if (isSensitiveOption(argument))
        {
            sanitized.append(argument);
            redactNextValue = true;
            continue;
        }

        const qsizetype equalsIndex = argument.indexOf(QLatin1Char('='));
        if (equalsIndex > 0 && isSensitiveOption(argument.first(equalsIndex)))
        {
            sanitized.append(argument.first(equalsIndex + 1) + redacted);
            continue;
        }

        sanitized.append(sanitizeUrl(argument));
    }

    return sanitized;
}

} // namespace LinuxGitShell
