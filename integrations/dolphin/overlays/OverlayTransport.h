// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QDBusMessage>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace LinuxGitShell::Dolphin
{

// Asynchronous channel to the overlay status service. Every call must return without waiting for
// the service; outcomes arrive later through the signals, already authenticated against the
// service owner the transport tracks.
class OverlayTransport : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;

    virtual void request(quint64 requestId, const QStringList& paths) = 0;
    virtual void release(const QStringList& paths) = 0;
    // Unsubscribes and abandons in-flight work, for example when overlays are disabled.
    virtual void stop() = 0;

  Q_SIGNALS:
    void requestAccepted(quint64 requestId, quint64 generation, const QStringList& acceptedPaths);
    // Transport failure, timeout, or a refusal such as InvalidRequest.
    void requestFailed(quint64 requestId);
    void initialResults(quint64 requestId, const QList<QVariantMap>& records, quint64 generation);
    void recordsChanged(const QList<QVariantMap>& records, quint64 generation);
    // The authenticated service owner disappeared or changed.
    void ownerLost();
};

// Talks to org.linuxgitshell.Experimental.Overlays1 on the session bus. It never uses generated
// proxies, which resolve the name owner synchronously. The owner is learned by an asynchronous
// GetNameOwner after the first valid reply (never by a blocking lookup) and every signal from
// another sender is dropped.
class DBusOverlayTransport final : public OverlayTransport
{
    Q_OBJECT

  public:
    explicit DBusOverlayTransport(QObject* parent = nullptr);
    DBusOverlayTransport(const QString& serviceName, QObject* parent);

    void request(quint64 requestId, const QStringList& paths) override;
    void release(const QStringList& paths) override;
    void stop() override;

  private Q_SLOTS:
    void onOverlayResults(const QDBusMessage& message);
    void onOverlaysChanged(const QDBusMessage& message);
    void onNameOwnerChanged(const QDBusMessage& message);

  private:
    struct EarlyMessage
    {
        bool isInitial = false;
        QString sender;
        quint64 requestId = 0;
        QList<QVariantMap> records;
        quint64 generation = 0;
    };

    void subscribe();
    void resolveOwner(quint64 requestId, quint64 callEpoch, quint64 generation,
                      const QStringList& acceptedPaths);
    void adoptOwner(const QString& sender);
    void deliver(const EarlyMessage& message);
    [[nodiscard]] static bool decode(const QDBusMessage& message, bool isInitial,
                                     EarlyMessage& decoded);

    static constexpr qsizetype MaxEarlyMessages = 32;

    QString serviceName;
    QString owner;
    bool subscribed = false;
    quint64 epoch = 0;
    QList<EarlyMessage> early;
};

} // namespace LinuxGitShell::Dolphin
