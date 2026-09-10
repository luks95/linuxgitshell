// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QMainWindow>
#include <QString>

namespace LinuxGitShell
{

class MainWindow final : public QMainWindow
{
  public:
    explicit MainWindow(const QString& requestedPath = {}, QWidget* parent = nullptr);
};

} // namespace LinuxGitShell
