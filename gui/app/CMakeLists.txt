# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_library(
    linuxgitshell_app STATIC
    MainWindow.cpp
    MainWindow.h
)

add_library(LinuxGitShell::App ALIAS linuxgitshell_app)

target_include_directories(linuxgitshell_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
    linuxgitshell_app
    PUBLIC
        LinuxGitShell::GitCore
        Qt6::Widgets
    PRIVATE
        KF6::I18n
)

linuxgitshell_set_compiler_warnings(linuxgitshell_app)

add_executable(linuxgitshell main.cpp)

set_target_properties(
    linuxgitshell
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(
    linuxgitshell
    PRIVATE
        LinuxGitShell::App
        KF6::CoreAddons
        KF6::I18n
)

linuxgitshell_set_compiler_warnings(linuxgitshell)

install(TARGETS linuxgitshell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
