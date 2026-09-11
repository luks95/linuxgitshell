// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include "linuxgitshell/gitcore/ProjectInfo.h"

#include <KLocalizedString>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace LinuxGitShell
{
namespace
{

[[nodiscard]] QString repositoryErrorText(RepositoryDiscoveryError error)
{
    switch (error)
    {
    case RepositoryDiscoveryError::None:
        return {};
    case RepositoryDiscoveryError::NotRepository:
        return i18n("Not a Git repository.");
    case RepositoryDiscoveryError::GitUnavailable:
        return i18n("Git is not available.");
    case RepositoryDiscoveryError::PermissionDenied:
        return i18n("Permission denied while reading the repository.");
    case RepositoryDiscoveryError::InvalidOutput:
        return i18n("Git returned invalid repository information.");
    case RepositoryDiscoveryError::Cancelled:
        return i18n("Repository discovery was cancelled.");
    case RepositoryDiscoveryError::TimedOut:
        return i18n("Repository discovery timed out.");
    case RepositoryDiscoveryError::GitFailure:
        return i18n("Git failed while discovering the repository.");
    }
    return i18n("Unknown repository error.");
}

[[nodiscard]] QString statusErrorText(GitStatusReadError error)
{
    switch (error)
    {
    case GitStatusReadError::None:
        return {};
    case GitStatusReadError::NotRepository:
        return i18n("Status is unavailable because the path is not a Git repository.");
    case GitStatusReadError::NoWorkingTree:
        return i18n("Status is unavailable for a repository without a working tree.");
    case GitStatusReadError::GitUnavailable:
        return i18n("Git is not available for status inspection.");
    case GitStatusReadError::PermissionDenied:
        return i18n("Permission denied while reading status.");
    case GitStatusReadError::Cancelled:
        return i18n("Status inspection was cancelled.");
    case GitStatusReadError::TimedOut:
        return i18n("Status inspection timed out.");
    case GitStatusReadError::GitFailure:
        return i18n("Git failed while reading status.");
    case GitStatusReadError::ParseFailure:
        return i18n("Git returned an unsupported status response.");
    }
    return i18n("Unknown status error.");
}

[[nodiscard]] QString configErrorText(GitConfigReadError error)
{
    switch (error)
    {
    case GitConfigReadError::None:
        return {};
    case GitConfigReadError::GitUnavailable:
        return i18n("Git is not available for configuration inspection.");
    case GitConfigReadError::PermissionDenied:
        return i18n("Permission denied while reading configuration.");
    case GitConfigReadError::Cancelled:
        return i18n("Configuration inspection was cancelled.");
    case GitConfigReadError::TimedOut:
        return i18n("Configuration inspection timed out.");
    case GitConfigReadError::GitFailure:
        return i18n("Git failed while reading configuration.");
    case GitConfigReadError::ParseFailure:
        return i18n("Git returned an unsupported configuration response.");
    }
    return i18n("Unknown configuration error.");
}

[[nodiscard]] QString repositoryTypeText(RepositoryType type)
{
    switch (type)
    {
    case RepositoryType::Normal:
        return i18n("Normal repository");
    case RepositoryType::Bare:
        return i18n("Bare repository");
    case RepositoryType::LinkedWorktree:
        return i18n("Linked worktree");
    case RepositoryType::Submodule:
        return i18n("Submodule");
    }
    return {};
}

[[nodiscard]] QString displayOutput(QByteArray output)
{
    output.replace('\0', "\\0\n");
    return QString::fromLocal8Bit(output).trimmed();
}

} // namespace

MainWindow::MainWindow(const QString& requestedPath, QWidget* parent)
    : QMainWindow(parent), requestedPath(requestedPath)
{
    setWindowTitle(ProjectInfo::name());
    resize(820, 640);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);

    auto* title = new QLabel(QStringLiteral("<h1>%1</h1>").arg(ProjectInfo::name()), content);
    stateLabel = new QLabel(i18n("Waiting for a repository path."), content);
    stateLabel->setObjectName(QStringLiteral("stateLabel"));
    pathLabel = new QLabel(content);
    pathLabel->setObjectName(QStringLiteral("pathLabel"));
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    reloadButton = new QPushButton(i18n("Reload"), content);
    reloadButton->setObjectName(QStringLiteral("reloadButton"));
    connect(reloadButton, &QPushButton::clicked, this, &MainWindow::startLoading);

    auto* repositoryGroup = new QGroupBox(i18n("Repository"), content);
    auto* repositoryLayout = new QFormLayout(repositoryGroup);
    rootValue = new QLabel(repositoryGroup);
    rootValue->setObjectName(QStringLiteral("rootValue"));
    rootValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    branchValue = new QLabel(repositoryGroup);
    branchValue->setObjectName(QStringLiteral("branchValue"));
    repositoryTypeValue = new QLabel(repositoryGroup);
    repositoryTypeValue->setObjectName(QStringLiteral("repositoryTypeValue"));
    repositoryLayout->addRow(i18n("Root:"), rootValue);
    repositoryLayout->addRow(i18n("Branch:"), branchValue);
    repositoryLayout->addRow(i18n("Type:"), repositoryTypeValue);

    auto* statusGroup = new QGroupBox(i18n("Status"), content);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusValue = new QLabel(statusGroup);
    statusValue->setObjectName(QStringLiteral("statusValue"));
    statusLayout->addWidget(statusValue);

    auto* configGroup = new QGroupBox(i18n("Configuration"), content);
    auto* configLayout = new QVBoxLayout(configGroup);
    configSummary = new QLabel(configGroup);
    configSummary->setObjectName(QStringLiteral("configSummary"));
    configTable = new QTreeWidget(configGroup);
    configTable->setObjectName(QStringLiteral("configTable"));
    configTable->setColumnCount(4);
    configTable->setHeaderLabels({i18n("Scope"), i18n("Origin"), i18n("Key"), i18n("Value")});
    configTable->setRootIsDecorated(false);
    configTable->setAlternatingRowColors(true);
    configTable->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    configTable->header()->setStretchLastSection(true);
    configLayout->addWidget(configSummary);
    configLayout->addWidget(configTable);

    auto* diagnosticGroup = new QGroupBox(i18n("Git diagnostic output"), content);
    diagnosticGroup->setObjectName(QStringLiteral("diagnosticGroup"));
    auto* diagnosticLayout = new QVBoxLayout(diagnosticGroup);
    diagnosticOutput = new QPlainTextEdit(diagnosticGroup);
    diagnosticOutput->setObjectName(QStringLiteral("diagnosticOutput"));
    diagnosticOutput->setReadOnly(true);
    diagnosticLayout->addWidget(diagnosticOutput);
    diagnosticGroup->setVisible(false);

    auto* versions =
        new QLabel(i18n("LinuxGitShell %1 · Qt %2 · KDE Frameworks %3", ProjectInfo::version(),
                        ProjectInfo::qtVersion(), ProjectInfo::kfVersion()),
                   content);

    layout->addWidget(title);
    layout->addWidget(stateLabel);
    layout->addWidget(pathLabel);
    layout->addWidget(reloadButton, 0, Qt::AlignLeft);
    layout->addWidget(repositoryGroup);
    layout->addWidget(statusGroup);
    layout->addWidget(configGroup, 1);
    layout->addWidget(diagnosticGroup);
    layout->addWidget(versions);

    setCentralWidget(content);

    connect(&discovery, &RepositoryDiscovery::finished, this, &MainWindow::discoveryFinished);
    connect(&statusReader, &GitStatusReader::finished, this, &MainWindow::statusFinished);
    connect(&configReader, &GitConfigReader::finished, this, &MainWindow::configFinished);
    QTimer::singleShot(0, this, &MainWindow::startLoading);
}

void MainWindow::startLoading()
{
    resetView();
    pathLabel->setText(requestedPath.isEmpty() ? i18n("No repository path was provided.")
                                               : i18n("Requested path: %1", requestedPath));
    if (requestedPath.isEmpty())
    {
        stateLabel->setText(i18n("Provide a repository path on the command line to inspect it."));
        reloadButton->setEnabled(false);
        Q_EMIT loadFinished(false);
        return;
    }

    stateLabel->setText(i18n("Loading repository…"));
    reloadButton->setEnabled(false);
    switch (discovery.discover(requestedPath))
    {
    case RepositoryDiscoveryStartResult::Accepted:
        return;
    case RepositoryDiscoveryStartResult::Busy:
        finishDiscoveryFailure(i18n("Repository discovery is already running."));
        return;
    case RepositoryDiscoveryStartResult::InvalidPath:
        finishDiscoveryFailure(i18n("Invalid repository path."));
        return;
    }
}

void MainWindow::discoveryFinished(const RepositoryDiscoveryResult& result)
{
    if (!result.repository.has_value())
    {
        finishDiscoveryFailure(repositoryErrorText(result.error), result.gitResults);
        return;
    }

    const RepositoryInfo& repository = *result.repository;
    updateRepository(repository);
    stateLabel->setText(i18n("Reading status and configuration…"));

    if (repository.type == RepositoryType::Bare)
    {
        statusValue->setText(i18n("Bare repository · working tree status is unavailable."));
        statusComplete = true;
    }
    else
    {
        GitStatusRequest statusRequest;
        statusRequest.path = repository.workTree;
        if (statusReader.start(statusRequest) != GitStatusStartResult::Accepted)
        {
            statusValue->setText(i18n("Could not start status inspection."));
            statusComplete = true;
            loadFailed = true;
        }
    }

    GitConfigRequest configRequest;
    configRequest.path = repository.repositoryRoot;
    if (configReader.start(configRequest) != GitConfigStartResult::Accepted)
    {
        configSummary->setText(i18n("Could not start configuration inspection."));
        configComplete = true;
        loadFailed = true;
    }
    checkLoadFinished();
}

void MainWindow::statusFinished(const GitStatusResult& result)
{
    statusComplete = true;
    if (result.status.has_value())
    {
        updateStatus(*result.status);
    }
    else
    {
        loadFailed = true;
        statusValue->setText(statusErrorText(result.error));
        appendDiagnostic(i18n("Status inspection"), result.gitResult);
    }
    checkLoadFinished();
}

void MainWindow::configFinished(const GitConfigResult& result)
{
    configComplete = true;
    if (result.config.has_value())
    {
        updateConfig(*result.config);
    }
    else
    {
        loadFailed = true;
        configSummary->setText(configErrorText(result.error));
        appendDiagnostic(i18n("Configuration inspection"), result.gitResult);
    }
    checkLoadFinished();
}

void MainWindow::updateRepository(const RepositoryInfo& repository)
{
    rootValue->setText(repository.repositoryRoot);
    repositoryTypeValue->setText(repositoryTypeText(repository.type));

    QString branch = repository.isDetachedHead ? i18n("Detached HEAD") : repository.currentBranch;
    if (branch.isEmpty())
    {
        branch = i18n("No branch");
    }
    if (!repository.upstream.isEmpty())
    {
        branch += i18n(" · upstream %1", repository.upstream);
    }
    if (repository.aheadCount.has_value() && repository.behindCount.has_value())
    {
        branch += i18n(" · ahead %1, behind %2", *repository.aheadCount, *repository.behindCount);
    }
    branchValue->setText(branch);
}

void MainWindow::updateStatus(const GitStatusSnapshot& status)
{
    if (status.isClean())
    {
        statusValue->setText(i18n("Working tree clean"));
        return;
    }

    qsizetype staged = 0;
    qsizetype modified = 0;
    qsizetype untracked = 0;
    qsizetype conflicts = 0;
    for (const GitStatusEntry& entry : status.entries)
    {
        if (entry.isConflicted())
        {
            ++conflicts;
            continue;
        }
        if (entry.kind == GitStatusRecordKind::Untracked)
        {
            ++untracked;
            continue;
        }
        if (entry.kind == GitStatusRecordKind::Ignored)
        {
            continue;
        }
        if (entry.indexState != GitFileState::Unmodified)
        {
            ++staged;
        }
        if (entry.workTreeState != GitFileState::Unmodified)
        {
            ++modified;
        }
    }
    statusValue->setText(i18n("Staged: %1 · Modified: %2 · Untracked: %3 · Conflicts: %4", staged,
                              modified, untracked, conflicts));
}

void MainWindow::updateConfig(const GitConfigSnapshot& config)
{
    configTable->clear();
    for (const GitConfigEntry& entry : config.entries)
    {
        auto* item = new QTreeWidgetItem({entry.scopeName, entry.origin, entry.key, entry.value});
        configTable->addTopLevelItem(item);
    }
    configSummary->setText(i18n("Configuration entries: %1", config.entries.size()));
}

void MainWindow::appendDiagnostic(const QString& context, const GitProcessResult& result)
{
    QStringList details;
    if (!result.errorString.isEmpty())
    {
        details.append(i18n("Process error: %1", result.errorString));
    }
    const QString standardError = displayOutput(result.standardError);
    if (!standardError.isEmpty())
    {
        details.append(i18n("Standard error:\n%1", standardError));
    }
    const QString standardOutput = displayOutput(result.standardOutput);
    if (!standardOutput.isEmpty())
    {
        details.append(i18n("Standard output:\n%1", standardOutput));
    }
    if (details.isEmpty())
    {
        return;
    }

    if (!diagnosticOutput->toPlainText().isEmpty())
    {
        diagnosticOutput->appendPlainText(QString{});
    }
    diagnosticOutput->appendPlainText(context + QStringLiteral("\n") +
                                      details.join(QLatin1Char('\n')));
    diagnosticOutput->parentWidget()->setVisible(true);
}

void MainWindow::checkLoadFinished()
{
    if (!statusComplete || !configComplete)
    {
        return;
    }
    stateLabel->setText(loadFailed ? i18n("Repository loaded with errors")
                                   : i18n("Repository loaded"));
    reloadButton->setEnabled(true);
    Q_EMIT loadFinished(!loadFailed);
}

void MainWindow::finishDiscoveryFailure(const QString& message,
                                        const QList<GitProcessResult>& diagnostics)
{
    loadFailed = true;
    stateLabel->setText(message);
    for (const GitProcessResult& diagnostic : diagnostics)
    {
        appendDiagnostic(i18n("Repository discovery"), diagnostic);
    }
    reloadButton->setEnabled(true);
    Q_EMIT loadFinished(false);
}

void MainWindow::resetView()
{
    statusComplete = false;
    configComplete = false;
    loadFailed = false;
    rootValue->clear();
    branchValue->clear();
    repositoryTypeValue->clear();
    statusValue->clear();
    configSummary->clear();
    configTable->clear();
    diagnosticOutput->clear();
    diagnosticOutput->parentWidget()->setVisible(false);
}

} // namespace LinuxGitShell
