// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <KAbstractFileItemActionPlugin>

#include <QList>
#include <QVariantList>

class QAction;
class KFileItemListProperties;
class QWidget;

namespace LinuxGitShell::Dolphin
{

class GitActionPlugin final : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

  public:
    GitActionPlugin(QObject* parent, const QVariantList& arguments);

    [[nodiscard]] QList<QAction*> actions(const KFileItemListProperties& fileItemInfos,
                                          QWidget* parentWidget) override;
};

} // namespace LinuxGitShell::Dolphin
