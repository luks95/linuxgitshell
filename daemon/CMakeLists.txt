# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_library(
    linuxgitshell_daemon_core STATIC
    ContextService.cpp
    ContextService.h
    ContextServiceAdaptor.cpp
    ContextServiceAdaptor.h
    DaemonLogging.cpp
    DaemonLogging.h
)

add_library(LinuxGitShell::DaemonCore ALIAS linuxgitshell_daemon_core)

target_include_directories(linuxgitshell_daemon_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
    linuxgitshell_daemon_core
    PUBLIC
        LinuxGitShell::GitCore
        LinuxGitShell::RepositoryContext
        Qt6::DBus
)

linuxgitshell_set_compiler_warnings(linuxgitshell_daemon_core)

add_executable(linuxgitshell-daemon main.cpp)

set_target_properties(
    linuxgitshell-daemon
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(linuxgitshell-daemon PRIVATE LinuxGitShell::DaemonCore)

linuxgitshell_set_compiler_warnings(linuxgitshell-daemon)

install(TARGETS linuxgitshell-daemon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# D-Bus activation starts the service on the first asynchronous request.
set(LINUXGITSHELL_DAEMON_EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/linuxgitshell-daemon")
configure_file(
    org.linuxgitshell.Experimental.Context1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.linuxgitshell.Experimental.Context1.service"
    @ONLY
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.linuxgitshell.Experimental.Context1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
)
