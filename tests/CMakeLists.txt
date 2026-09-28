# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_executable(ProjectInfoTest ProjectInfoTest.cpp)

target_link_libraries(
    ProjectInfoTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(ProjectInfoTest)

add_test(NAME gitcore.project_info COMMAND ProjectInfoTest)

add_executable(GitProcessTestHelper GitProcessTestHelper.cpp)
target_link_libraries(GitProcessTestHelper PRIVATE Qt6::Core)
linuxgitshell_set_compiler_warnings(GitProcessTestHelper)

add_executable(GitProcessRunnerTest GitProcessRunnerTest.cpp)

target_link_libraries(
    GitProcessRunnerTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

add_dependencies(GitProcessRunnerTest GitProcessTestHelper)
linuxgitshell_set_compiler_warnings(GitProcessRunnerTest)

add_test(NAME gitcore.process_runner COMMAND GitProcessRunnerTest)

add_executable(RepositoryDiscoveryTest RepositoryDiscoveryTest.cpp)

target_compile_definitions(
    RepositoryDiscoveryTest
    PRIVATE GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
)

target_link_libraries(
    RepositoryDiscoveryTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(RepositoryDiscoveryTest)

add_test(NAME gitcore.repository_discovery COMMAND RepositoryDiscoveryTest)

add_executable(GitStatusParserTest GitStatusParserTest.cpp)

target_link_libraries(
    GitStatusParserTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(GitStatusParserTest)

add_test(NAME gitcore.status_parser COMMAND GitStatusParserTest)

add_executable(GitStatusReaderTest GitStatusReaderTest.cpp)

target_compile_definitions(
    GitStatusReaderTest
    PRIVATE GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
)

target_link_libraries(
    GitStatusReaderTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(GitStatusReaderTest)

add_test(NAME gitcore.status_reader COMMAND GitStatusReaderTest)

add_executable(GitConfigTest GitConfigTest.cpp)

target_compile_definitions(
    GitConfigTest
    PRIVATE GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
)

target_link_libraries(
    GitConfigTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(GitConfigTest)

add_test(NAME gitcore.config COMMAND GitConfigTest)

add_executable(RepositoryContextCacheTest RepositoryContextCacheTest.cpp)

target_link_libraries(
    RepositoryContextCacheTest
    PRIVATE
        LinuxGitShell::RepositoryContext
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(RepositoryContextCacheTest)

add_test(NAME repositorycontext.cache COMMAND RepositoryContextCacheTest)

add_executable(RepositoryContextWireTest RepositoryContextWireTest.cpp)

target_link_libraries(
    RepositoryContextWireTest
    PRIVATE
        LinuxGitShell::RepositoryContext
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(RepositoryContextWireTest)

add_test(NAME repositorycontext.wire COMMAND RepositoryContextWireTest)

add_executable(ContextServiceTest ContextServiceTest.cpp GitTestSupport.h)

target_compile_definitions(
    ContextServiceTest
    PRIVATE GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
)

target_link_libraries(
    ContextServiceTest
    PRIVATE
        LinuxGitShell::DaemonCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(ContextServiceTest)

add_test(NAME daemon.context_service COMMAND ContextServiceTest)

find_program(DBUS_RUN_SESSION_EXECUTABLE NAMES dbus-run-session REQUIRED)

set(context_dbus_interface_sources)
qt_add_dbus_interface(
    context_dbus_interface_sources
    "${PROJECT_SOURCE_DIR}/dbus/org.linuxgitshell.Experimental.Context1.xml"
    contextinterface
)

add_executable(
    ContextDBusTest
    ContextDBusTest.cpp
    GitTestSupport.h
    ${context_dbus_interface_sources}
)

target_compile_definitions(
    ContextDBusTest
    PRIVATE
        GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
        DAEMON_EXECUTABLE_PATH="$<TARGET_FILE:linuxgitshell-daemon>"
        DBUS_RUN_SESSION_PATH="${DBUS_RUN_SESSION_EXECUTABLE}"
)

target_include_directories(ContextDBusTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(
    ContextDBusTest
    PRIVATE
        LinuxGitShell::RepositoryContext
        Qt6::DBus
        Qt6::Test
)

add_dependencies(ContextDBusTest linuxgitshell-daemon)
linuxgitshell_set_compiler_warnings(ContextDBusTest)

add_test(
    NAME daemon.context_dbus
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- "$<TARGET_FILE:ContextDBusTest>"
)

add_executable(MainWindowTest MainWindowTest.cpp)

target_compile_definitions(
    MainWindowTest
    PRIVATE GIT_EXECUTABLE_PATH="${GIT_EXECUTABLE}"
)

target_link_libraries(
    MainWindowTest
    PRIVATE
        LinuxGitShell::App
        Qt6::Test
        KF6::I18n
)

linuxgitshell_set_compiler_warnings(MainWindowTest)

add_test(NAME app.main_window COMMAND MainWindowTest)
set_tests_properties(app.main_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TranslationTest TranslationTest.cpp)

target_compile_definitions(
    TranslationTest
    PRIVATE TRANSLATION_LOCALE_DIR="${CMAKE_BINARY_DIR}/po"
)

target_link_libraries(
    TranslationTest
    PRIVATE
        Qt6::Test
        KF6::I18n
)

add_dependencies(TranslationTest linuxgitshell_translations)
linuxgitshell_set_compiler_warnings(TranslationTest)

add_test(NAME i18n.spanish_catalog COMMAND TranslationTest)
set_tests_properties(
    i18n.spanish_catalog
    PROPERTIES ENVIRONMENT "LC_ALL=es_PY.UTF-8;LANGUAGE=es"
)

add_executable(DolphinContextMenuSelectionTest DolphinContextMenuSelectionTest.cpp)

target_link_libraries(
    DolphinContextMenuSelectionTest
    PRIVATE
        LinuxGitShell::DolphinContext
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(DolphinContextMenuSelectionTest)

add_test(NAME dolphin.context_selection COMMAND DolphinContextMenuSelectionTest)

add_executable(DolphinPluginLaunchHelper DolphinPluginLaunchHelper.cpp)
target_link_libraries(DolphinPluginLaunchHelper PRIVATE Qt6::Core)
linuxgitshell_set_compiler_warnings(DolphinPluginLaunchHelper)

add_executable(DolphinContextMenuPluginTest DolphinContextMenuPluginTest.cpp)

target_compile_definitions(
    DolphinContextMenuPluginTest
    PRIVATE
        DOLPHIN_CONTEXT_PLUGIN_PATH="$<TARGET_FILE:linuxgitshell_fileitemaction>"
        DOLPHIN_PLUGIN_LAUNCH_HELPER_PATH="$<TARGET_FILE:DolphinPluginLaunchHelper>"
)

target_link_libraries(
    DolphinContextMenuPluginTest
    PRIVATE
        Qt6::Test
        KF6::CoreAddons
        KF6::KIOWidgets
)

add_dependencies(
    DolphinContextMenuPluginTest
    linuxgitshell_fileitemaction
    DolphinPluginLaunchHelper
)
linuxgitshell_set_compiler_warnings(DolphinContextMenuPluginTest)

add_test(NAME dolphin.context_plugin COMMAND DolphinContextMenuPluginTest)
set_tests_properties(
    dolphin.context_plugin
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LANGUAGE=C;LC_ALL=C"
)
