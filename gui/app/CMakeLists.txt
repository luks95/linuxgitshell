# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_executable(
    linuxgitshell
    MainWindow.cpp
    MainWindow.h
    main.cpp
)

set_target_properties(
    linuxgitshell
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(
    linuxgitshell
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
)

linuxgitshell_set_compiler_warnings(linuxgitshell)

install(TARGETS linuxgitshell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
