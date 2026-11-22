# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

# The overlay client holds the cache, request scheduling, and D-Bus transport. The KIO plugin built
# from it is deliberately thin because getOverlays() runs on Dolphin's paint path.
add_library(
    linuxgitshell_dolphin_overlays STATIC
    OverlayClient.cpp
    OverlayClient.h
    OverlayTransport.cpp
    OverlayTransport.h
)

add_library(LinuxGitShell::DolphinOverlays ALIAS linuxgitshell_dolphin_overlays)

set_target_properties(
    linuxgitshell_dolphin_overlays
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    linuxgitshell_dolphin_overlays
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    linuxgitshell_dolphin_overlays
    PUBLIC
        LinuxGitShell::RepositoryContext
        Qt6::Core
        Qt6::DBus
)

linuxgitshell_set_compiler_warnings(linuxgitshell_dolphin_overlays)

kcoreaddons_add_plugin(
    linuxgitshell_overlayicon
    SOURCES
        OverlayIconPlugin.cpp
        OverlayIconPlugin.h
    INSTALL_NAMESPACE "kf6/overlayicon"
)

target_link_libraries(
    linuxgitshell_overlayicon
    PRIVATE
        LinuxGitShell::DolphinOverlays
        KF6::ConfigCore
        KF6::KIOCore
)

linuxgitshell_set_compiler_warnings(linuxgitshell_overlayicon)
