# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_library(
    linuxgitshell_dolphin_context STATIC
    ContextMenuSelection.cpp
    ContextMenuSelection.h
    RepositoryContextClient.cpp
    RepositoryContextClient.h
    RepositoryContextTransport.cpp
    RepositoryContextTransport.h
)

add_library(LinuxGitShell::DolphinContext ALIAS linuxgitshell_dolphin_context)

set_target_properties(
    linuxgitshell_dolphin_context
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    linuxgitshell_dolphin_context
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    linuxgitshell_dolphin_context
    PUBLIC
        LinuxGitShell::RepositoryContext
        Qt6::Core
        Qt6::DBus
)

linuxgitshell_set_compiler_warnings(linuxgitshell_dolphin_context)

kcoreaddons_add_plugin(
    linuxgitshell_fileitemaction
    SOURCES
        GitActionPlugin.cpp
        GitActionPlugin.h
    INSTALL_NAMESPACE "kf6/kfileitemaction"
)

target_link_libraries(
    linuxgitshell_fileitemaction
    PRIVATE
        LinuxGitShell::DolphinContext
        KF6::I18n
        KF6::KIOWidgets
)

linuxgitshell_set_compiler_warnings(linuxgitshell_fileitemaction)
