// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextServiceAdaptor.h"

#include "ContextService.h"

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

namespace LinuxGitShell
{

ContextServiceAdaptor::ContextServiceAdaptor(ContextService* service)
    : QDBusAbstractAdaptor(service), service(service)
{
    connect(service, &ContextService::contextReady, this,
            [this](const QString& pathKey, const RepositoryContextSnapshot& snapshot,
                   quint64 generation)
            {
                Q_EMIT ContextReady(pathKey, repositoryContextToVariantMap(snapshot),
                                    static_cast<qulonglong>(generation));
            });
}

qulonglong ContextServiceAdaptor::RequestContext(const QStringList& paths)
{
    return static_cast<qulonglong>(service->requestContext(paths));
}

} // namespace LinuxGitShell
