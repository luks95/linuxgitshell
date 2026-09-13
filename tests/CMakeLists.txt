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
