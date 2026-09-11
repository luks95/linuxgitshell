// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitConfigReader.h"
#include "linuxgitshell/gitcore/GitStatusReader.h"
#include "linuxgitshell/gitcore/RepositoryDiscovery.h"

#include <QMainWindow>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QWidget;

namespace LinuxGitShell
{

class MainWindow final : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(const QString& requestedPath = {}, QWidget* parent = nullptr);

  Q_SIGNALS:
    void loadFinished(bool succeeded);

  private:
    void startLoading();
    void discoveryFinished(const RepositoryDiscoveryResult& result);
    void statusFinished(const GitStatusResult& result);
    void configFinished(const GitConfigResult& result);
    void updateRepository(const RepositoryInfo& repository);
    void updateStatus(const GitStatusSnapshot& status);
    void updateConfig(const GitConfigSnapshot& config);
    void appendDiagnostic(const QString& context, const GitProcessResult& result);
    void checkLoadFinished();
    void finishDiscoveryFailure(const QString& message,
                                const QList<GitProcessResult>& diagnostics = {});
    void resetView();

    QString requestedPath;
    RepositoryDiscovery discovery;
    GitStatusReader statusReader;
    GitConfigReader configReader;
    QLabel* stateLabel = nullptr;
    QLabel* pathLabel = nullptr;
    QLabel* rootValue = nullptr;
    QLabel* branchValue = nullptr;
    QLabel* repositoryTypeValue = nullptr;
    QLabel* statusValue = nullptr;
    QLabel* configSummary = nullptr;
    QTreeWidget* configTable = nullptr;
    QPlainTextEdit* diagnosticOutput = nullptr;
    QPushButton* reloadButton = nullptr;
    bool statusComplete = false;
    bool configComplete = false;
    bool loadFailed = false;
};

} // namespace LinuxGitShell
