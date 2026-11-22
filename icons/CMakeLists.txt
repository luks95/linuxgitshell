# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

# Overlay emblems for the Dolphin overlay plugin. Their names are the contract with
# OverlayClient::iconNameFor(); themes can override them.
install(
    FILES
        hicolor/scalable/emblems/emblem-linuxgitshell-added.svg
        hicolor/scalable/emblems/emblem-linuxgitshell-clean.svg
        hicolor/scalable/emblems/emblem-linuxgitshell-conflict.svg
        hicolor/scalable/emblems/emblem-linuxgitshell-modified.svg
        hicolor/scalable/emblems/emblem-linuxgitshell-untracked.svg
    DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/emblems"
)
