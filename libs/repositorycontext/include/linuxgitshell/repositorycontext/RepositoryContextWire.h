// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <QVariantMap>

#include <optional>

namespace LinuxGitShell
{

// Encodes a snapshot as the extensible `a{sv}` dictionary carried by the experimental
// org.linuxgitshell.Experimental.Context1 D-Bus interface. Enumerations travel as strings so that
// clients can ignore values and keys they do not know.
[[nodiscard]] QVariantMap repositoryContextToVariantMap(const RepositoryContextSnapshot& snapshot);

// Returns no snapshot when the state is missing or unknown, or when an inside-repository snapshot
// has no root or an unknown type. Unknown keys and operations are ignored.
[[nodiscard]] std::optional<RepositoryContextSnapshot>
repositoryContextFromVariantMap(const QVariantMap& map);

} // namespace LinuxGitShell
