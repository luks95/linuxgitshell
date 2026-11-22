// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include <QLatin1StringView>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace LinuxGitShell
{

// Names of the experimental overlay interface in dbus/org.linuxgitshell.Experimental.Overlays1.xml.
inline constexpr QLatin1StringView OverlayServiceName("org.linuxgitshell.Experimental.Overlays1");
inline constexpr QLatin1StringView OverlayObjectPath("/org/linuxgitshell/Overlays");
inline constexpr QLatin1StringView OverlayInterfaceName("org.linuxgitshell.Experimental.Overlays1");
inline constexpr QLatin1StringView
    OverlayInvalidRequestError("org.linuxgitshell.Experimental.Overlays1.Error.InvalidRequest");

// One answer for one observed path. `path` is the caller's lexical request key.
struct OverlayRecord
{
    QString path;
    QString repositoryRoot;
    RepositoryStatusState state = RepositoryStatusState::Unknown;
    RepositoryStatusValidity validity = RepositoryStatusValidity::Pending;
    quint64 revision = 0;
    // Remaining freshness of a Ready record; zero for every other validity.
    quint32 validForMs = 0;
};

// Enumerations travel as strings so that clients can reject values they do not know.
[[nodiscard]] QString overlayStateName(RepositoryStatusState state);
[[nodiscard]] QString overlayValidityName(RepositoryStatusValidity validity);

// Encodes a record as the extensible `a{sv}` dictionary carried by the overlay interface.
[[nodiscard]] QVariantMap overlayRecordToVariantMap(const OverlayRecord& record);

// Returns nothing when a required field is missing or has the wrong type, or when the state or
// validity is unknown: such a record must remove any previously cached icon. Unknown optional keys
// are ignored. A non-Ready record is normalized to Unknown with no freshness.
[[nodiscard]] std::optional<OverlayRecord> overlayRecordFromVariantMap(const QVariantMap& map);

// Conservative encoded-size estimate used to keep each message below the 1 MiB ceiling.
[[nodiscard]] qsizetype overlayRecordEncodedSizeEstimate(const OverlayRecord& record);

// Splits records into batches of at most `maxRecords` and `maxBytes` (estimated). A single record
// larger than `maxBytes` still travels alone. Never returns an empty batch.
[[nodiscard]] QList<QList<OverlayRecord>>
splitOverlayRecords(const QList<OverlayRecord>& records, qsizetype maxRecords, qsizetype maxBytes);

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::OverlayRecord)
Q_DECLARE_METATYPE(QList<LinuxGitShell::OverlayRecord>)
